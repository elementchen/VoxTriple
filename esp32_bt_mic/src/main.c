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
#include "sdkconfig.h"

#include "bt_init.h"
#include "ble_gatts_config.h"
#include "audio_capture.h"
#include "button_handler.h"
#include "config_storage.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  ESP32 Bluetooth Microphone - Dual Mode");
    ESP_LOGI(TAG, "  HFP AG + BLE GATT Server");
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

    /* Step 5: Initialize BLE GATT server */
    ESP_LOGI(TAG, "Step 5: Initializing BLE GATT server...");
    ble_gatts_init();

    /* Step 6: Initialize button handler */
    ESP_LOGI(TAG, "Step 6: Initializing button handler...");
    button_handler_init();

    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  System initialized successfully!");
    ESP_LOGI(TAG, "  Waiting for Bluetooth connections...");
    ESP_LOGI(TAG, "============================================");

    /* Main task done - other tasks handle everything */
    /* Keep main task alive (optional, could delete) */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
