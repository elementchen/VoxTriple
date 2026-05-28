/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_wifi.h"

#include "bt_init.h"
#include "ble_gatts_config.h"
#include "audio_capture.h"
#include "button_handler.h"
#include "config_storage.h"
#include "ble_hid_keyboard.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    /* Drive indicator LED low immediately so it stays off until a button press. */
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << GPIO_NUM_18),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);
    gpio_set_level(GPIO_NUM_18, 0);

    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  ESP32 Bluetooth Microphone - PTT Mode");
    ESP_LOGI(TAG, "  HFP HF Client + BLE GATT Server");
    ESP_LOGI(TAG, "============================================");

    /* Step 1: Initialize NVS */
    ESP_LOGI(TAG, "Step 1: Initializing NVS...");
    ESP_ERROR_CHECK(bt_nvs_init());

    /* Step 2: Load saved configuration */
    ESP_LOGI(TAG, "Step 2: Loading configuration...");
    config_storage_init();

    /* Step 3: Initialize I2S microphone driver */
    ESP_LOGI(TAG, "Step 3: Initializing I2S microphone...");
    ESP_ERROR_CHECK(audio_capture_init());

    /* Step 4: Initialize Bluetooth stack (dual mode: BTDM) */
    ESP_LOGI(TAG, "Step 4: Initializing Bluetooth stack...");
    ESP_ERROR_CHECK(bt_stack_init());

    /* Load and apply saved BT sleep mode setting.
     * Default to enabled for new devices. */
    uint8_t sleep_mode = 1;
    if (config_storage_load_sleep_mode(&sleep_mode) == ESP_OK && sleep_mode) {
        esp_bt_sleep_enable();
        ESP_LOGI(TAG, "BT sleep mode enabled");
    } else {
        esp_bt_sleep_disable();
        ESP_LOGI(TAG, "BT sleep mode disabled");
    }

    /* Disable WiFi to save power — we only use Bluetooth */
    esp_err_t wifi_ret = esp_wifi_stop();
    if (wifi_ret == ESP_OK) {
        esp_wifi_deinit();
        ESP_LOGI(TAG, "WiFi disabled for power saving");
    }

    /* Step 5a: Initialize BLE HID Keyboard first (registers its GATT callback) */
    ESP_LOGI(TAG, "Step 5a: Initializing BLE HID Keyboard...");
    ESP_ERROR_CHECK(ble_hid_keyboard_init());

    /* Step 5b: Initialize BLE GATT server (overwrites callback with unified
     * handler that dispatches to HID handler internally) */
    ESP_LOGI(TAG, "Step 5b: Initializing BLE GATT server...");
    ble_gatts_init();

    /* Step 6: Initialize button handler */
    ESP_LOGI(TAG, "Step 6: Initializing button handler...");
    button_handler_init();

    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  System initialized successfully!");
    ESP_LOGI(TAG, "  Waiting for Bluetooth connections...");
    ESP_LOGI(TAG, "============================================");

    /* Suppress all INFO logs to reduce serial traffic and CPU load.
     * Only WARN/ERROR will appear. Comment this line to restore debug logs. */
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set("BTN_HANDLER", ESP_LOG_DEBUG);
    esp_log_level_set("BLE_GATTS", ESP_LOG_DEBUG);

    /* Main task done - other tasks handle everything */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
