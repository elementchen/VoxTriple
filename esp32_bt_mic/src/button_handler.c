/*
 * SPDX-FileCopyrightText: 2024 VoxTriple Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "button_handler.h"
#include "ble_gatts_config.h"
#include "config_storage.h"
#include "bt_hfp_hf.h"
#include "bt_init.h"
#include "config_storage.h"
#include "ws2812_led.h"
#include "esp_hf_client_api.h"

#ifndef CONFIG_BUTTON_4_GPIO
#define CONFIG_BUTTON_4_GPIO  27
#endif

static const char *TAG = "BTN_HANDLER";

/* Button GPIO configuration */
#define DEBOUNCE_MS          50
#define LONG_PRESS_MS        1000
#define BTN4_LONG_PRESS_MS   5000    /* Button 4 long-press: clear + pairing */
#define PAIRING_TIMEOUT_MS   30000   /* 30s pairing window */

/* Pairing-mode state (kept across polling ticks) */
static bool  s_pairing_active  = false;
static bool  s_pairing_got_dev = false;
static uint32_t s_pairing_start  = 0;
#define BUTTON_TASK_STACK    4096
#define BUTTON_TASK_PRIORITY 3

/* PTT buttons (1-3) + device switch (4) */
#define PTT_NUM             3

static const gpio_num_t s_ptt_pins[PTT_NUM] = {
    CONFIG_BUTTON_1_GPIO,
    CONFIG_BUTTON_2_GPIO,
    CONFIG_BUTTON_3_GPIO,
};

static const gpio_num_t BTN4_PIN = CONFIG_BUTTON_4_GPIO;

typedef enum {
    BTN_STATE_IDLE,
    BTN_STATE_DEBOUNCE,
    BTN_STATE_PRESSED,
} btn_state_t;

static TaskHandle_t s_btn_task_handle = NULL;
static bool s_btn_task_running = false;

static void do_pairing_mode(void);
static void do_device_switch(void);

/* ── Device Switch ───────────────────────────────────────────── */
static void do_device_switch(void)
{
    if (s_pairing_active) {
        /* Cancel pairing mode */
        ESP_LOGI(TAG, "Pairing cancelled by user");
        s_pairing_active = false;
        s_pairing_got_dev = false;
        ws2812_blink_stop();
        return;
    }

    int cur = config_storage_get_active_device();
    int next = (cur + 1) % MAX_DEVICES;
    ESP_LOGI(TAG, "Switching device %d → %d", cur, next);

    /* Disconnect current so new device can find us */
    if (bt_audio_is_active()) bt_hfp_hf_ptt_release();
    if (ble_gatts_is_connected()) ble_gatts_disconnect();
    if (bt_hfp_is_connected()) bt_hfp_disconnect();

    /* Switch active index */
    config_storage_set_active_device(next);

    /* Show device indicator */
    ws2812_device_indicator(next);

    /* Restart BLE advertising (will also make Classic BT discoverable
     * if it was stopped).  The new device will auto-reconnect both
     * HFP and BLE when it sees us. */
    ble_gatts_adv_stop();
    vTaskDelay(pdMS_TO_TICKS(200));
    ble_gatts_adv_start();

    ESP_LOGI(TAG, "Device %d now active, waiting for auto-reconnect", next);
}

/* ── Pairing Mode (non-blocking, cancellable) ─────────────────── */
static void do_pairing_mode(void)
{
    if (s_pairing_active) return;
    ESP_LOGI(TAG, "Entering pairing mode");
    s_pairing_active = true;
    s_pairing_got_dev = false;
    s_pairing_start = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /* Disconnect so we're fully available */
    if (bt_audio_is_active()) bt_hfp_hf_ptt_release();
    if (ble_gatts_is_connected()) ble_gatts_disconnect();
    if (bt_hfp_is_connected()) bt_hfp_disconnect();

    /* Blue fast blink */
    ws2812_blink_color(0, 0, 64, 120);
}

/* Called from polling loop when pairing is active */
static void pairing_tick(void)
{
    if (!s_pairing_active) return;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /* Check if device connected */
    if (!s_pairing_got_dev && bt_hfp_is_connected()) {
        s_pairing_got_dev = true;
        int idx = config_storage_get_active_device();
        extern esp_bd_addr_t hf_peer_addr;
        config_storage_save_device(idx, hf_peer_addr);
        ESP_LOGI(TAG, "Device saved to slot %d", idx);
        ws2812_blink_stop();
        ws2812_solid_color(0, 64, 0, 3000);
        s_pairing_active = false;
        return;
    }

    /* Timeout: just stop blinking, blue off */
    if ((now - s_pairing_start) > PAIRING_TIMEOUT_MS) {
        ESP_LOGI(TAG, "Pairing timeout — exiting");
        ws2812_blink_stop();
        s_pairing_active = false;
    }
}

/**
 * @brief Button monitoring task with debounce
 */
static void button_task_func(void *arg)
{
    btn_state_t state[PTT_NUM];
    btn_state_t btn4_state = BTN_STATE_IDLE;
    uint32_t press_time[PTT_NUM];
    uint32_t btn4_press_time = 0;
    uint32_t last_change[PTT_NUM];
    uint32_t btn4_last_change = 0;
    uint32_t now;

    memset(state, 0, sizeof(state));
    memset(press_time, 0, sizeof(press_time));
    memset(last_change, 0, sizeof(last_change));

    ESP_LOGI(TAG, "Button task started (PTT: %d,%d,%d | SW: %d)",
             s_ptt_pins[0], s_ptt_pins[1], s_ptt_pins[2], BTN4_PIN);

    while (s_btn_task_running) {
        now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* ── PTT Buttons 1-3 ── */
        for (int i = 0; i < PTT_NUM; i++) {
            int level = gpio_get_level(s_ptt_pins[i]);
            int pressed = (level == 0);

            switch (state[i]) {
            case BTN_STATE_IDLE:
                if (pressed) {
                    state[i] = BTN_STATE_DEBOUNCE;
                    last_change[i] = now;
                }
                break;

            case BTN_STATE_DEBOUNCE:
                if (!pressed) {
                    state[i] = BTN_STATE_IDLE;
                } else if ((now - last_change[i]) >= DEBOUNCE_MS) {
                    state[i] = BTN_STATE_PRESSED;
                    press_time[i] = now;
                    ESP_LOGI(TAG, "Button %d pressed", i + 1);

                    if (i == 0) {
                        bt_hfp_hf_ptt_press();
                        if (ble_gatts_is_connected()) {
                            ws2812_rainbow_start();
                        } else {
                            ws2812_blink_red(3);
                        }
                    }

                    /* BLE notify only if connected */
                    if (ble_gatts_is_connected()) {
                        ble_send_button_event(i, 1);
                    }
                }
                break;

            case BTN_STATE_PRESSED:
                if (!pressed) {
                    uint32_t duration = now - press_time[i];
                    ESP_LOGI(TAG, "Button %d released (duration: %lu ms)", i + 1, duration);

                    if (i == 0) {
                        bt_hfp_hf_ptt_release();
                        ws2812_rainbow_stop();
                    }

                    if (ble_gatts_is_connected()) {
                        ble_send_button_event(i, 0);
                    }

                    state[i] = BTN_STATE_IDLE;
                }
                break;
            }
        }

        /* ── Button 4: Device Switch ── */
        {
            int level = gpio_get_level(BTN4_PIN);
            int pressed = (level == 0);

            switch (btn4_state) {
            case BTN_STATE_IDLE:
                if (pressed) {
                    btn4_state = BTN_STATE_DEBOUNCE;
                    btn4_last_change = now;
                }
                break;

            case BTN_STATE_DEBOUNCE:
                if (!pressed) {
                    btn4_state = BTN_STATE_IDLE;
                } else if ((now - btn4_last_change) >= DEBOUNCE_MS) {
                    btn4_state = BTN_STATE_PRESSED;
                    btn4_press_time = now;
                    ESP_LOGI(TAG, "Button 4 (SW) pressed");
                }
                break;

            case BTN_STATE_PRESSED:
            {
                uint32_t duration = now - btn4_press_time;
                /* Long-press triggers IMMEDIATELY when 5s reached, even if still held */
                if (duration >= BTN4_LONG_PRESS_MS) {
                    ESP_LOGI(TAG, "Button 4 long-press triggered (clear device + pairing)");
                    int cur = config_storage_get_active_device();
                    config_storage_clear_device(cur);
                    ESP_LOGI(TAG, "Cleared device %d pairing record", cur);
                    do_pairing_mode();
                    btn4_state = BTN_STATE_IDLE; /* reset after trigger */
                    break;
                }
                if (!pressed) {
                    ESP_LOGI(TAG, "Button 4 released (duration: %lu ms)", duration);
                    do_device_switch();
                    btn4_state = BTN_STATE_IDLE;
                }
                break;
            }
            }
        }

        /* Pairing-mode state machine (non-blocking) */
        if (s_pairing_active) pairing_tick();

        vTaskDelay(pdMS_TO_TICKS(10));  /* 10ms polling */
    }

    ESP_LOGI(TAG, "Button task stopped");
    vTaskDelete(NULL);
}

void button_handler_init(void)
{
    ESP_LOGI(TAG, "Initializing button handler");

    /* Configure PTT buttons 1-3 */
    gpio_config_t io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    for (int i = 0; i < PTT_NUM; i++)
        io_conf.pin_bit_mask |= (1ULL << s_ptt_pins[i]);
    io_conf.pin_bit_mask |= (1ULL << BTN4_PIN);

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Start button monitoring task */
    s_btn_task_running = true;
    xTaskCreate(button_task_func, "BtnTask", BUTTON_TASK_STACK,
                NULL, BUTTON_TASK_PRIORITY, &s_btn_task_handle);

    ESP_LOGI(TAG, "Button handler initialized (4 buttons)");
}

void button_handler_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing button handler");
    s_btn_task_running = false;
    if (s_btn_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(50));
        s_btn_task_handle = NULL;
    }
}
