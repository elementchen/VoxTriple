/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "bt_init.h"
#include "audio_capture.h"
#include "button_handler.h"
#include "config_storage.h"
#include "classic_hidd.h"
#include "uart_console.h"
#include "bt_config.h"
#include "board_profile.h"
#include "multi_host.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  ESP32 Bluetooth Microphone - PTT Mode");
    ESP_LOGI(TAG, "  HFP HF Client + BLE GATT Server");
    ESP_LOGI(TAG, "============================================");

    /* Step 1: Initialize NVS */
    ESP_LOGI(TAG, "Step 1: Initializing NVS...");
    ESP_ERROR_CHECK(bt_nvs_init());

    /* Step 2: Load saved configuration & Board Profile */
    ESP_LOGI(TAG, "Step 2: Loading configuration & Board Profile...");
    config_storage_init();
    board_profile_init();

    const board_profile_t *profile = board_profile_get_current();
    gpio_num_t led_pin = profile ? profile->led_gpio : GPIO_NUM_16;

    /* Drive indicator LED high immediately (OFF for active-low LED) */
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << led_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);
    gpio_set_level(led_pin, 1); // 1 = OFF (Active Low)

    /* Check if we need to enter clean OTA mode */
    uint8_t ota_ready = 0;
    if (config_storage_load_ota_ready(&ota_ready) == ESP_OK && ota_ready == 1) {
        ESP_LOGW(TAG, "=========================================================");
        ESP_LOGW(TAG, "   DETECTED OTA READY FLAG! ENTERING PURE OTA MODE        ");
        ESP_LOGW(TAG, "   SKIPPING ALL BT, AUDIO, & GPIO INITIALIZATIONS        ");
        ESP_LOGW(TAG, "=========================================================");
        
        // 1. Initialize only Wired UART Console for firmware reception
        ESP_ERROR_CHECK(uart_console_init());
        
        // 2. Light up board LED to show visual OTA indication (0: ON)
        gpio_set_level(led_pin, 0); // ON (Active Low)
        
        // 3. Keep main task idle while UART receives firmware
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    /* Dump NVS bt_config namespace */
    ESP_LOGI(TAG, "Dumping all NVS entries:");
    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find("nvs", NULL, NVS_TYPE_ANY, &it);
    if (err != ESP_OK || it == NULL) {
        ESP_LOGI(TAG, "  No entries found in NVS partition 'nvs'");
    }
    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        ESP_LOGI(TAG, "  NS: %s, Key: %s, Type: 0x%02X", info.namespace_name, info.key, info.type);
        err = nvs_entry_next(&it);
        if (err != ESP_OK) {
            it = NULL;
        }
    }

    /* Read and print bt_cfg_key0 */
    nvs_handle_t handle;
    if (nvs_open("bt_config.conf", NVS_READONLY, &handle) == ESP_OK) {
        size_t size = 0;
        if (nvs_get_blob(handle, "bt_cfg_key0", NULL, &size) == ESP_OK && size > 0) {
            char *buf = malloc(size + 1);
            if (buf) {
                if (nvs_get_blob(handle, "bt_cfg_key0", buf, &size) == ESP_OK) {
                    buf[size] = '\0';
                    ESP_LOGI(TAG, "Content of bt_cfg_key0 (size=%d):", size);
                    /* Print in chunks if it is too long for ESP_LOG */
                    for (int i = 0; i < size; i += 256) {
                        printf("%.256s", buf + i);
                    }
                    printf("\n");
                }
                free(buf);
            }
        }
        nvs_close(handle);
    }

    /* Step 3: Initialize I2S microphone driver */
    ESP_LOGI(TAG, "Step 3: Initializing I2S microphone...");
#if ENABLE_CLASSIC_BT_MIC
    ESP_ERROR_CHECK(audio_capture_init());
#else
    ESP_LOGI(TAG, "I2S microphone skipped (Classic BT disabled)");
#endif

    /* Step 4: Initialize Bluetooth stack (dual mode: BTDM) */
    ESP_LOGI(TAG, "Step 4: Initializing Bluetooth stack...");
    ESP_ERROR_CHECK(bt_stack_init());

    /* Force disable BT sleep mode (Modem Sleep) in runtime to prevent connection timeouts (reason 546) and MIC failures */
    esp_bt_sleep_disable();
    ESP_LOGI(TAG, "BT sleep mode disabled by force");

    /* Disable WiFi to save power — we only use Bluetooth */
    esp_err_t wifi_ret = esp_wifi_stop();
    if (wifi_ret == ESP_OK) {
        esp_wifi_deinit();
        ESP_LOGI(TAG, "WiFi disabled for power saving");
    }

    /* Step 5: Initialize Classic BT HID Keyboard */
    ESP_LOGI(TAG, "Step 5: Initializing Classic BT HID Keyboard...");
    ESP_ERROR_CHECK(classic_hidd_init());

    /* Step 5b: Initialize Wired UART Console Config */
    ESP_LOGI(TAG, "Step 5b: Initializing Wired UART Console Config...");
    ESP_ERROR_CHECK(uart_console_init());

    /* Step 6: Initialize button handler */
    ESP_LOGI(TAG, "Step 6: Initializing button handler...");
    button_handler_init();

    /* Step 7: Initialize Multi-Host Manager */
    ESP_LOGI(TAG, "Step 7: Initializing Multi-Host Manager...");
    multi_host_init();
    uint8_t curr_slot = multi_host_get_current_slot();

    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  System initialized successfully!");
    ESP_LOGI(TAG, "  Firmware: v1.0.18 (Dual-Host Ready)");
    ESP_LOGI(TAG, "  Active Slot: [%d] (Switch via Btn 3+4)", curr_slot);
    ESP_LOGI(TAG, "  Waiting for Bluetooth connections...");
    ESP_LOGI(TAG, "============================================");

    /* Restore INFO logging for debugging BT stack state and connection event flow. */
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("BTN_HANDLER", ESP_LOG_DEBUG);
    esp_log_level_set("CLASSIC_HIDD", ESP_LOG_DEBUG);
    esp_log_level_set("BT_BTM", ESP_LOG_DEBUG);
    esp_log_level_set("BT_GAP", ESP_LOG_DEBUG);
    esp_log_level_set("BT_HCI", ESP_LOG_DEBUG);
    esp_log_level_set("BT_APPL", ESP_LOG_DEBUG);
    esp_log_level_set("BT_BTC", ESP_LOG_DEBUG);

    /* Main task done - other tasks handle everything */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
