/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * BLE HID Keyboard — NimBLE stack, based on official esp_hid_device example.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_hidd.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_hs_id.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"

#include "bt_init.h"
#include "esp_hid_gap.h"
#include "button_handler.h"
#include "config_storage.h"
#include "ble_hid_keyboard.h"

static const char *TAG = "MAIN";

void ble_store_config_init(void);

static ble_hs_sync_fn *s_orig_sync_cb;

static void nimble_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    ESP_LOGI(TAG, "NimBLE synced (rc=%d), starting advertising", rc);
    esp_hid_ble_gap_adv_start();
}

/* Stub — encryption complete, our button_handler sends HID reports directly */
void ble_hid_task_start_up(void)
{
    ESP_LOGI(TAG, "Encryption complete, HID ready for button input");
}

/* NimBLE host task */
static void nimble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* HID device event callback */
static void hid_event_cb(void *handler_args, esp_event_base_t base,
                         int32_t id, void *event_data)
{
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (id) {
    case ESP_HIDD_START_EVENT:
        esp_hid_ble_gap_adv_start();
        break;
    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "HID connected");
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "HID disconnected");
        break;
    case ESP_HIDD_OUTPUT_EVENT:
        break;
    default:
        break;
    }
}

void app_main(void)
{
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
    ESP_LOGI(TAG, "  ESP32 BLE HID Keyboard — NimBLE");
    ESP_LOGI(TAG, "============================================");

    ESP_LOGI(TAG, "Step 1: Initializing NVS...");
    ESP_ERROR_CHECK(bt_nvs_init());

    ESP_LOGI(TAG, "Step 2: Loading configuration...");
    config_storage_init();

    /* Disable WiFi */
    esp_wifi_stop();
    esp_wifi_deinit();

    ESP_LOGI(TAG, "Step 3: Initializing NimBLE GAP...");
    esp_hid_gap_init(HID_DEV_MODE);

    /* Generate device name with MAC suffix */
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    char dev_name[32];
    snprintf(dev_name, sizeof(dev_name), "ESP32_KB2_%02X", mac[5]);

    ESP_LOGI(TAG, "Step 4: Configuring advertising + security...");
    esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, dev_name);

    /* Set GAP service name + appearance so Windows shows correct name + keyboard icon */
    ble_svc_gap_device_name_set(dev_name);
    ble_svc_gap_device_appearance_set(ESP_HID_APPEARANCE_KEYBOARD);

    /* Override key distribution: Windows requires ENC key in Key Distribution
     * to persist bonding in system registry across reboots. */
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ESP_LOGI(TAG, "Step 5: Initializing HID Keyboard...");
    ble_hid_keyboard_init();

    ESP_LOGI(TAG, "Step 6: Enabling NimBLE host...");
    ble_store_config_init();
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sync_cb = nimble_on_sync;
    esp_nimble_enable(nimble_host_task);

    ESP_LOGI(TAG, "Step 7: Initializing button handler...");
    button_handler_init();

    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  System initialized — waiting for HID start");
    ESP_LOGI(TAG, "============================================");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
