/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_ag_api.h"
#include "bt_init.h"
#include "bt_app_core.h"
#include "bt_app_hf.h"

static const char *TAG = "BT_INIT";

#define BT_APP_EVT_STACK_UP  0

static bool s_hfp_connected = false;
static bool s_audio_active = false;

/* Default device name - can be overridden by Kconfig */
#ifndef CONFIG_BT_MIC_DEVICE_NAME
#define CONFIG_BT_MIC_DEVICE_NAME "ESP32_BT_MIC"
#endif

static const char local_device_name[] = CONFIG_BT_MIC_DEVICE_NAME;

bool bt_hfp_is_connected(void)
{
    return s_hfp_connected;
}

bool bt_audio_is_active(void)
{
    return s_audio_active;
}

void bt_hfp_set_connected(bool connected)
{
    s_hfp_connected = connected;
}

void bt_audio_set_active(bool active)
{
    s_audio_active = active;
}

static void bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT: {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "authentication success: %s", param->auth_cmpl.device_name);
            ESP_LOG_BUFFER_HEX(TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        } else {
            ESP_LOGE(TAG, "authentication failed, status: %d", param->auth_cmpl.stat);
        }
        break;
    }
    case ESP_BT_GAP_PIN_REQ_EVT: {
        ESP_LOGI(TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
        esp_bt_pin_code_t pin_code;
        if (param->pin_req.min_16_digit) {
            ESP_LOGI(TAG, "Input pin code: 0000 0000 0000 0000");
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        } else {
            ESP_LOGI(TAG, "Input pin code: 0000");
            pin_code[0] = '0';
            pin_code[1] = '0';
            pin_code[2] = '0';
            pin_code[3] = '0';
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
        }
        break;
    }
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06" PRIu32,
                 param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey: %06" PRIu32, param->key_notif.passkey);
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(TAG, "ESP_BT_GAP_MODE_CHG_EVT mode: %d", param->mode_chg.mode);
        break;
    default:
        ESP_LOGI(TAG, "GAP event: %d", event);
        break;
    }
}

static void bt_stack_up_handler(uint16_t event, void *p_param)
{
    ESP_LOGD(TAG, "%s evt %d", __func__, event);

    switch (event) {
    case BT_APP_EVT_STACK_UP: {
        /* Set device name for Classic BT */
        esp_bt_gap_set_device_name(local_device_name);

        /* Register GAP callback */
        esp_bt_gap_register_callback(bt_gap_cb);

        /* Set Class of Device for Audio Gateway (HFP AG)
         * CoD: Major Service Class: Audio (bit 21) + Telephony (bit 18)
         *       Major Class: Phone (0x02)
         *       Minor Class: Common Mode (0x00)
         * This identifies the device as a Phone/Audio Gateway
         */
        esp_bt_cod_t cod;
        cod.service = 0x0024;  // Audio (0x20) + Telephony (0x04) services
        cod.major = 0x02;      // Phone major class
        cod.minor = 0x00;      // Common Mode minor class
        esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_SERVICE_CLASS | ESP_BT_SET_COD_MAJOR_MINOR);

        /* Register HFP AG callback and initialize */
        esp_hf_ag_register_callback(bt_app_hf_cb);
        esp_hf_ag_init();

        /* Set pairing PIN */
        esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
        esp_bt_pin_code_t pin_code;
        pin_code[0] = '0';
        pin_code[1] = '0';
        pin_code[2] = '0';
        pin_code[3] = '0';
        esp_bt_gap_set_pin(pin_type, 4, pin_code);

        /* Set discoverable and connectable */
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

        ESP_LOGI(TAG, "BT stack initialized, name: %s", local_device_name);
        break;
    }
    default:
        ESP_LOGE(TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}

esp_err_t bt_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t bt_stack_init(void)
{
    esp_err_t ret;

    /* Release Classic BT memory (we don't need BLE-only RAM) */
    /* Note: Don't call mem_release for BTDM dual mode */

    /* Initialize BT controller */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    /* Initialize Bluedroid */
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Bluetooth controller mode: BTDM (dual mode)");

    /* Start BT application task */
    bt_app_task_start_up();

    /* Dispatch stack-up event */
    bt_app_work_dispatch(bt_stack_up_handler, BT_APP_EVT_STACK_UP, NULL, 0, NULL);

    return ESP_OK;
}
