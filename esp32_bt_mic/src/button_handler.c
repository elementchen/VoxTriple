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
#include "ws2812_led.h"

#ifndef CONFIG_BUTTON_4_GPIO
#define CONFIG_BUTTON_4_GPIO  27
#endif

static const char *TAG = "BTN_HANDLER";

/* Button GPIO configuration */
#define DEBOUNCE_MS          50
#define LONG_PRESS_MS        1000
#define BTN4_LONG_PRESS_MS   2000    /* Button 4 long-press: pairing mode */
#define PAIRING_TIMEOUT_MS   30000   /* 30s pairing window */
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

static void do_device_switch(void)
{
    ESP_LOGI(TAG, "Device switch requested");
    /* Show device indicator: index 0 = 1 blink ×3, index 1 = 2 blinks ×3 */
    ws2812_device_indicator(0);  /* TODO: replace 0 with actual active_dev */
}

static void do_pairing_mode(void)
{
    ESP_LOGI(TAG, "Entering pairing mode (30s window)");
    ws2812_blink_color(0, 0, 64, 100);  /* blue fast blink */

    /* Stop current connections */
    if (bt_hfp_is_connected()) bt_hfp_disconnect();
    if (ble_gatts_is_connected()) ble_gatts_disconnect();
    /* Audio off if active */
    if (bt_audio_is_active()) {
        bt_hfp_hf_ptt_release();
    }

    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    bool paired = false;

    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start) < PAIRING_TIMEOUT_MS) {
        /* Wait for new connection on either HFP or BLE */
        if (bt_hfp_is_connected() || ble_gatts_is_connected()) {
            paired = true;
            ESP_LOGI(TAG, "New device paired");
            /* TODO: save to NVS device list */
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    ws2812_blink_stop();
    if (paired) {
        ws2812_solid_color(0, 64, 0, 3000);  /* green solid 3s */
    } else {
        ESP_LOGW(TAG, "Pairing timeout — no device found");
        ws2812_blink_red(2);
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
                if (!pressed) {
                    uint32_t duration = now - btn4_press_time;
                    ESP_LOGI(TAG, "Button 4 released (duration: %lu ms)", duration);

                    if (duration >= BTN4_LONG_PRESS_MS) {
                        do_pairing_mode();
                    } else {
                        do_device_switch();
                    }

                    btn4_state = BTN_STATE_IDLE;
                }
                break;
            }
        }

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
