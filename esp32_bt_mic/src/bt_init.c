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
#include "esp_hf_client_api.h"
#include "esp_mac.h"
#include "bt_init.h"
#include "bt_app_core.h"
#include "bt_app_hf.h"
#include "config_storage.h"
#include "bt_config.h"

static const char *TAG = "BT_INIT";

#define BT_APP_EVT_STACK_UP  0

static bool s_hfp_connected = false;
static bool s_audio_active = false;

/* Default device name - can be overridden by Kconfig */
#ifndef CONFIG_BT_MIC_DEVICE_NAME
#define CONFIG_BT_MIC_DEVICE_NAME "ESP32_BT_MIC"
#endif

/* Dynamic device name includes MAC last byte for multi-board区分 */
char g_bt_device_name[32] = "ESP32_BT_MIC";

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
#if 0
        /* 提早配置并注册本端 BLE 静态随机地址，确保后 5 字节与物理 MAC 一致以兼容硬件层接收过滤 */
        esp_bd_addr_t ble_mac;
        if (esp_read_mac(ble_mac, ESP_MAC_BT) == ESP_OK) {
            ble_mac[0] = 0xC0; // 符合静态随机地址规范（最高两位为 11）
            ble_mac[5] ^= 2;   // 暂时异或 2 以修改 MAC 地址，彻底绕过 Windows 对旧 MAC 的绑定缓存锁死进行诊断
            esp_err_t err = esp_ble_gap_set_rand_addr(ble_mac);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "BLE Static Random MAC set to %02X:%02X:%02X:%02X:%02X:%02X",
                         ble_mac[0], ble_mac[1], ble_mac[2], ble_mac[3], ble_mac[4], ble_mac[5]);
            } else {
                ESP_LOGE(TAG, "Set BLE rand address failed: %s", esp_err_to_name(err));
            }
        }
#endif

#if ENABLE_CLASSIC_BT_MIC
        /* Set device name for Classic BT */
        esp_bt_gap_set_device_name(g_bt_device_name);

        /* Register GAP callback */
        esp_bt_gap_register_callback(bt_gap_cb);

        uint8_t mic_enabled = 1;
        config_storage_load_mic_enabled(&mic_enabled);

        /* Set Class of Device
         * If mic enabled: Audio + Capturing + Telephony COD (0x340), Major 0x04 Video/Audio, Minor 0x02 Hands-free.
         * If mic disabled: Major 0x05 Peripheral, Minor 0x40 Keyboard, COD Service 0x000 (standard keyboard profile).
         */
        esp_bt_cod_t cod;
        if (mic_enabled) {
            cod.service = 0x340;  // Audio(0x100) + Capturing(0x040) + Telephony(0x200)
            cod.major = 0x04;     // Audio/Video
            cod.minor = 0x02;     // Hands-free Device
            esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_SERVICE_CLASS | ESP_BT_SET_COD_MAJOR_MINOR);

            /* Register HFP HF Client callback and initialize */
            esp_hf_client_register_callback(bt_app_hf_client_cb);
            esp_hf_client_init();
            ESP_LOGI(TAG, "HFP Client initialized successfully (Mic Enabled Mode)");
        } else {
            cod.service = 0x000;
            cod.major = 0x05;     // Peripheral
            cod.minor = 0x10;     // Keyboard (0x10 in 6-bit field translates to 0x40 in COD representation)
            esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_SERVICE_CLASS | ESP_BT_SET_COD_MAJOR_MINOR);
            ESP_LOGI(TAG, "HFP Client initialization skipped (Keyboard Only Mode)");
        }

        /* Set pairing PIN */
        esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
        esp_bt_pin_code_t pin_code;
        pin_code[0] = '0';
        pin_code[1] = '0';
        pin_code[2] = '0';
        pin_code[3] = '0';
        esp_bt_gap_set_pin(pin_type, 4, pin_code);

        /* Set Classic BT TX power: load from NVS or default to 0 dBm */
        uint8_t tx_power_level = 4;  /* Default: 0 dBm */
        if (config_storage_load_tx_power(&tx_power_level) != ESP_OK || tx_power_level > 7) {
            tx_power_level = 7;
        }
        esp_bredr_tx_power_set((esp_power_level_t)tx_power_level, (esp_power_level_t)tx_power_level);
        ESP_LOGI(TAG, "Classic BT TX power set to level %d", tx_power_level);

        /* Set classic BT non-discoverable and non-connectable initially */
        esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

        ESP_LOGI(TAG, "BT stack initialized, name: %s (Classic BT hidden initially)", g_bt_device_name);
#else
        ESP_LOGI(TAG, "BT stack initialized (Classic BT disabled entirely)");
#endif
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

    /* Release BLE controller memory to save RAM (~30KB) and strictly enforce BR/EDR-only mode */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    /* Initialize BT controller in BR/EDR-only mode */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
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

    /* Set scan mode to connectable & discoverable initially so Windows can find/reconnect classic BT */
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    /* Classic BT compound device name */
    uint8_t mic_enabled = 1;
    config_storage_load_mic_enabled(&mic_enabled);
    const uint8_t *addr = esp_bt_dev_get_address();
    if (addr) {
        if (mic_enabled) {
            snprintf(g_bt_device_name, sizeof(g_bt_device_name),
                     "ESP32_BT_KBD_MIC_%02X", addr[5]);
        } else {
            snprintf(g_bt_device_name, sizeof(g_bt_device_name),
                     "ESP32_BT_KBD_KEY_%02X", addr[5]);
        }
    }
    ESP_LOGI(TAG, "Bluetooth controller mode: Classic BT Only (BR/EDR), Name: %s", g_bt_device_name);

    /* Start BT application task */
    bt_app_task_start_up();

    /* Dispatch stack-up event */
    bt_app_work_dispatch(bt_stack_up_handler, BT_APP_EVT_STACK_UP, NULL, 0, NULL);

    return ESP_OK;
}

static bool s_classic_bt_activated = false;
static uint32_t s_last_activate_time = 0;
static uint32_t s_last_deactivate_time = 0;

void bt_classic_activate(void)
{
    uint8_t mic_enabled = 1;
    config_storage_load_mic_enabled(&mic_enabled);
    if (!mic_enabled) {
        ESP_LOGW(TAG, "Mic disabled in config, ignoring audio activation request.");
        return;
    }

    if (s_classic_bt_activated) return;

    /* 频控防护：上一次挂断与本次重新建立之间必须间隔至少 1000ms，防抖防爆 */
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - s_last_deactivate_time < 1000) {
        ESP_LOGW(TAG, "PTT activation ignored: click interval too short (<1000ms)");
        return;
    }

    s_last_activate_time = now;
    s_classic_bt_activated = true;
    ESP_LOGI(TAG, "PTT Button Pressed. Activating MIC (SCO)...");

    esp_bd_addr_t saved_addr = {0};
    if (config_storage_load_hfp_addr(saved_addr) == ESP_OK) {
        if (!bt_hfp_is_connected()) {
            ESP_LOGI(TAG, "HFP control link not connected. Connecting HFP first...");
            esp_hf_client_connect(saved_addr);
        } else {
            ESP_LOGI(TAG, "HFP control link connected. Direct connecting SCO channel...");
            esp_hf_client_connect_audio(saved_addr);
        }
    } else {
        ESP_LOGW(TAG, "No saved peer address for MIC activation. Please pair keyboard first.");
    }
}

void bt_classic_deactivate(void)
{
    if (!s_classic_bt_activated) return;

    /* 延迟释放：若建立连接与本次挂断之间短于 800ms，则延迟执行，防止连接尚未完全建立就执行销毁引发底层死锁 */
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - s_last_activate_time < 800) {
        uint32_t delay_ms = 800 - (now - s_last_activate_time);
        ESP_LOGI(TAG, "Deferring SCO disconnect by %" PRIu32 " ms for link stability...", delay_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }

    s_last_deactivate_time = now;
    s_classic_bt_activated = false;
    ESP_LOGI(TAG, "PTT Button Released. Deactivating MIC (SCO)...");

    esp_bd_addr_t saved_addr = {0};
    if (config_storage_load_hfp_addr(saved_addr) == ESP_OK) {
        ESP_LOGI(TAG, "Disconnecting active classic BT HFP SCO channel...");
        esp_hf_client_disconnect_audio(saved_addr);
    }
}

bool bt_classic_is_ptt_activated(void)
{
    return s_classic_bt_activated;
}
