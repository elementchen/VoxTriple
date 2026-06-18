/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Minimal NimBLE init — replaces bt_init.c for BLE-only mode.
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

void ble_store_config_init(void);  /* forward decl: NimBLE NVS store init */
#include "bt_init.h"

static const char *TAG = "BT_INIT";

char g_bt_device_name[32] = "ESP32_KB";

/* Stubs for button_handler.c compatibility */
bool bt_hfp_is_connected(void) { return false; }
bool bt_audio_is_active(void)   { return false; }
void bt_hfp_set_connected(bool c) { (void)c; }
void bt_audio_set_active(bool a)  { (void)a; }
void bt_hfp_hf_wake_acl(void)     { }

esp_err_t bt_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/* NimBLE host task — runs the event loop */
static void nimble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t bt_stack_init(void)
{
    esp_err_t ret;

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_nimble_init();

    /* Generate device name with MAC suffix and set via NimBLE GAP */
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(g_bt_device_name, sizeof(g_bt_device_name), "ESP32_KB_%02X", mac[5]);
    ble_svc_gap_device_name_set(g_bt_device_name);

    /* Enable NVS-based bond persistence */
    ble_store_config_init();
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    esp_nimble_enable(nimble_host_task);

    /* Let NimBLE host sync before HID services register */
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "NimBLE BLE-only mode ready");
    return ESP_OK;
}
