/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
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

static const char *TAG = "BTN_HANDLER";

/* Button GPIO configuration */
#define DEBOUNCE_MS          50
#define LONG_PRESS_MS        1000
#define BUTTON_TASK_STACK    4096
#define BUTTON_TASK_PRIORITY 3

static const gpio_num_t s_button_pins[BUTTON_NUM] = {
    CONFIG_BUTTON_1_GPIO,
    CONFIG_BUTTON_2_GPIO,
    CONFIG_BUTTON_3_GPIO,
};

typedef enum {
    BTN_STATE_IDLE,
    BTN_STATE_DEBOUNCE,
    BTN_STATE_PRESSED,
} btn_state_t;

static TaskHandle_t s_btn_task_handle = NULL;
static bool s_btn_task_running = false;

/**
 * @brief Button monitoring task with debounce
 */
static void button_task_func(void *arg)
{
    btn_state_t state[BUTTON_NUM];
    uint32_t press_time[BUTTON_NUM];
    uint32_t last_change[BUTTON_NUM];
    uint32_t now;

    memset(state, 0, sizeof(state));
    memset(press_time, 0, sizeof(press_time));
    memset(last_change, 0, sizeof(last_change));

    ESP_LOGI(TAG, "Button task started (GPIOs: %d, %d, %d)",
             s_button_pins[0], s_button_pins[1], s_button_pins[2]);

    while (s_btn_task_running) {
        now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        for (int i = 0; i < BUTTON_NUM; i++) {
            int level = gpio_get_level(s_button_pins[i]);
            int pressed = (level == 0);  /* Active low: pressed = low */

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

                    /* BLE notification for keyboard shortcut — no SCO control */
                    ble_send_button_event(i, 1);
                }
                break;

            case BTN_STATE_PRESSED:
                if (!pressed) {
                    uint32_t duration = now - press_time[i];
                    ESP_LOGI(TAG, "Button %d released (duration: %lu ms)", i + 1, duration);

                    /* BLE notification for keyboard shortcut release */
                    ble_send_button_event(i, 0);

                    state[i] = BTN_STATE_IDLE;
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

    /* Configure GPIO pins as input with pull-up */
    gpio_config_t io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    for (int i = 0; i < BUTTON_NUM; i++) {
        io_conf.pin_bit_mask |= (1ULL << s_button_pins[i]);
    }

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Start button monitoring task */
    s_btn_task_running = true;
    xTaskCreate(button_task_func, "BtnTask", BUTTON_TASK_STACK,
                NULL, BUTTON_TASK_PRIORITY, &s_btn_task_handle);

    ESP_LOGI(TAG, "Button handler initialized");
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
