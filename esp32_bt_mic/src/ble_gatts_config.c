/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"
#include "esp_bt.h"
#include "esp_hidd.h"
#include "ble_gatts_config.h"
#include "config_storage.h"
#include "bt_init.h"
#include "esp_hf_client_api.h"

static const char *TAG = "BLE_GATTS";

/* ----------------------------------------------------------------
 * Service UUID 0x1820 (Unofficial - used for custom service)
 * Characteristic UUIDs: 0x2A01 - 0x2A05
 * ---------------------------------------------------------------- */
#define GATTS_SERVICE_UUID       0x1820
#define GATTS_CHAR_BTN1_MAP_UUID 0x2A01
#define GATTS_CHAR_BTN2_MAP_UUID 0x2A02
#define GATTS_CHAR_BTN3_MAP_UUID 0x2A03
#define GATTS_CHAR_BTN_EVENT_UUID 0x2A04
#define GATTS_CHAR_DEV_STATUS_UUID 0x2A05
#define GATTS_CHAR_TX_POWER_UUID   0x2A06
#define GATTS_CHAR_SLEEP_MODE_UUID 0x2A07
#define GATTS_CHAR_BTN4_MAP_UUID   0x2A08

#define GATTS_NUM_HANDLES    24
#define GATTS_APP_ID         0x01
#define PREPARE_BUF_MAX_SIZE 1024

extern char g_bt_device_name[];

/* Characteristic value lengths */
#define BTN_MAP_CHAR_LEN      2
#define BTN_EVENT_CHAR_LEN    2
#define DEV_STATUS_CHAR_LEN   2

typedef struct {
    uint8_t *prepare_buf;
    int      prepare_len;
} prepare_type_env_t;

static prepare_type_env_t s_prepare_write_env;

static esp_gatt_char_prop_t s_char_property = 0;

/* Characteristic attribute handles */
static uint16_t s_service_handle = 0;
static uint16_t s_btn1_map_handle = 0;
static uint16_t s_btn2_map_handle = 0;
static uint16_t s_btn3_map_handle = 0;
static uint16_t s_btn4_map_handle = 0;
static uint16_t s_btn_event_handle = 0;
static uint16_t s_dev_status_handle = 0;
static uint16_t s_tx_power_handle = 0;
static uint16_t s_sleep_mode_handle = 0;
static uint16_t s_btn_event_descr_handle = 0;
static uint16_t s_dev_status_descr_handle = 0;

/* Track the GATT interface */
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_conn_id = 0;
static bool s_ble_connected = false;

/* Current button mapping values */
static uint8_t s_btn1_map[BTN_MAP_LEN] = {0x0D, 0x00};  /* VK_RETURN */
static uint8_t s_btn2_map[BTN_MAP_LEN] = {0x1B, 0x00};  /* VK_ESCAPE */
static uint8_t s_btn3_map[BTN_MAP_LEN] = {0x20, 0x00};  /* VK_SPACE */
static uint8_t s_btn4_map[BTN_MAP_LEN] = {0x09, 0x00};  /* VK_TAB */
static uint8_t s_btn_event[BTN_EVENT_CHAR_LEN] = {0, 0};
static uint8_t s_dev_status[DEV_STATUS_CHAR_LEN] = {0, 0};
static uint8_t s_tx_power = 4;     /* Default: 0 dBm */
static uint8_t s_sleep_mode = 1;   /* Default: enabled */

/* Advertising parameters */
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x060,  /* 60 ms */
    .adv_int_max        = 0x060,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_RPA_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void ble_init_adv_data(const char *name)
{
    /* BLE SMP — Secure Connections bonding, no I/O capability. */
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, 1);
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, 1);

    /* 128-bit HID service UUID for advertising */
    const uint8_t hid_uuid128[] = {
        0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
    };

    /* Structured advertising — the stack manages 31-byte budget:
     * puts name in scan-rsp if needed, splits fields intelligently */
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp       = false,
        .include_name       = true,
        .include_txpower    = true,
        .appearance         = 0x03C1,  /* HID Keyboard */
        .min_interval       = 0x0006,
        .max_interval       = 0x0010,
        .manufacturer_len   = 0,
        .p_manufacturer_data = NULL,
        .service_data_len   = 0,
        .p_service_data     = NULL,
        .service_uuid_len   = sizeof(hid_uuid128),
        .p_service_uuid     = (uint8_t *)hid_uuid128,
        .flag               = ESP_BLE_ADV_FLAG_GEN_DISC,
    };

    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret) {
        ESP_LOGW(TAG, "config adv data failed (0x%x), using raw fallback", ret);
        /* Fallback: minimal raw advertising */
        uint8_t raw[31];
        int p = 0;
        raw[p++] = 0x02; raw[p++] = ESP_BLE_AD_TYPE_FLAG;
        raw[p++] = ESP_BLE_ADV_FLAG_GEN_DISC;
        raw[p++] = 0x03; raw[p++] = ESP_BLE_AD_TYPE_APPEARANCE;
        raw[p++] = 0xC1; raw[p++] = 0x03;
        raw[p++] = 17; raw[p++] = ESP_BLE_AD_TYPE_128SRV_CMPL;
        memcpy(&raw[p], hid_uuid128, 16); p += 16;
        ret = esp_ble_gap_config_adv_data_raw(raw, p);
        if (ret) {
            ESP_LOGE(TAG, "raw adv data failed: 0x%x", ret);
        }
    }

    /* Scan response: our custom 128-bit UUID for Python app discovery */
    uint8_t scan_rsp[18];
    scan_rsp[0] = 17;
    scan_rsp[1] = ESP_BLE_AD_TYPE_128SRV_CMPL;
    uint8_t uuid128[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                           0x00, 0x10, 0x00, 0x00, 0x20, 0x18, 0x00, 0x00};
    memcpy(&scan_rsp[2], uuid128, 16);
    ret = esp_ble_gap_config_scan_rsp_data_raw(scan_rsp, sizeof(scan_rsp));
    if (ret) {
        ESP_LOGE(TAG, "scan rsp data failed: 0x%x", ret);
    }
}

static void adv_retry_cb(TimerHandle_t xTimer)
{
    esp_ble_gap_start_advertising(&adv_params);
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        /* Start BLE advertising immediately. Both BLE keyboard and
         * Classic BT HFP are available simultaneously. BTDM security
         * conflicts are handled by auto-retry on disconnect. */
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising start failed");
        } else {
            ESP_LOGI(TAG, "BLE advertising started");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising stop failed");
        } else {
            ESP_LOGI(TAG, "BLE advertising stopped");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "update connection params status=%d conn_int=%d latency=%d timeout=%d",
                 param->update_conn_params.status,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;

    case ESP_GAP_BLE_LOCAL_IR_EVT:
    case ESP_GAP_BLE_LOCAL_ER_EVT:
        break;
    default:
        break;
    }
}

static void prepare_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    esp_gatt_status_t status = ESP_GATT_OK;

    if (param->write.need_rsp) {
        if (param->write.is_prep) {
            if (param->write.offset > PREPARE_BUF_MAX_SIZE) {
                status = ESP_GATT_INVALID_OFFSET;
            } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
                status = ESP_GATT_INVALID_ATTR_LEN;
            }

            if (status == ESP_GATT_OK && prepare_write_env->prepare_buf == NULL) {
                prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
                prepare_write_env->prepare_len = 0;
                if (prepare_write_env->prepare_buf == NULL) {
                    ESP_LOGE(TAG, "Gatt_server prep no mem");
                    status = ESP_GATT_NO_RESOURCES;
                }
            }

            esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
            if (gatt_rsp) {
                gatt_rsp->attr_value.len = param->write.len;
                gatt_rsp->attr_value.handle = param->write.handle;
                gatt_rsp->attr_value.offset = param->write.offset;
                gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
                memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
                esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                                                     param->write.trans_id, status, gatt_rsp);
                if (response_err != ESP_OK) {
                    ESP_LOGE(TAG, "Send response error");
                }
                free(gatt_rsp);
            } else {
                ESP_LOGE(TAG, "%s, malloc failed", __func__);
                status = ESP_GATT_NO_RESOURCES;
            }

            if (status != ESP_GATT_OK) {
                return;
            }
            memcpy(prepare_write_env->prepare_buf + param->write.offset,
                   param->write.value,
                   param->write.len);
            prepare_write_env->prepare_len += param->write.len;
        } else {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
        }
    }
}

static void exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
        ESP_LOG_BUFFER_HEX(TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
    } else {
        ESP_LOGI(TAG, "ESP_GATT_PREP_WRITE_CANCEL");
    }
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

static void add_characteristic(uint16_t service_handle, uint16_t *char_handle,
                               uint16_t uuid_val, esp_gatt_char_prop_t property,
                               uint8_t *initial_value, uint8_t value_len,
                               uint16_t *descr_handle)
{
    esp_bt_uuid_t char_uuid;
    char_uuid.len = ESP_UUID_LEN_16;
    char_uuid.uuid.uuid16 = uuid_val;

    esp_attr_value_t attr_val = {
        .attr_max_len = value_len,
        .attr_len     = value_len,
        .attr_value   = initial_value,
    };

    esp_err_t ret = esp_ble_gatts_add_char(service_handle, &char_uuid,
                                           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                           property, &attr_val, NULL);
    if (ret) {
        ESP_LOGE(TAG, "add char 0x%04X failed, error code = 0x%x", uuid_val, ret);
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* Dispatch to HID GATT handler first (for keyboard events) */
    esp_hidd_gatts_event_handler(event, gatts_if, param);

    switch (event) {
    case ESP_GATTS_REG_EVT: {
        ESP_LOGI(TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
        if (param->reg.status == ESP_GATT_OK) {
            s_gatts_if = gatts_if;
        } else {
            ESP_LOGE(TAG, "Reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
        esp_ble_gap_config_local_privacy(true);
        ble_init_adv_data(g_bt_device_name);

        /* Create service */
        esp_gatt_srvc_id_t service_id = {
            .is_primary = true,
            .id.inst_id = 0x00,
            .id.uuid.len = ESP_UUID_LEN_16,
            .id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID,
        };
        esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLES);
        break;
    }

    case ESP_GATTS_CREATE_EVT: {
        ESP_LOGI(TAG, "CREATE_SERVICE_EVT, status %d, service_handle %d",
                 param->create.status, param->create.service_handle);
        s_service_handle = param->create.service_handle;

        s_char_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

        /* Add Button 1 Map characteristic */
        config_storage_load_button(BUTTON_ID_1, &s_btn1_map[0], &s_btn1_map[1]);
        add_characteristic(s_service_handle, &s_btn1_map_handle,
                           GATTS_CHAR_BTN1_MAP_UUID, s_char_property,
                           s_btn1_map, BTN_MAP_LEN, NULL);
        break;
    }

    case ESP_GATTS_ADD_CHAR_EVT: {
        ESP_LOGI(TAG, "ADD_CHAR_EVT, status %d, attr_handle %d",
                 param->add_char.status, param->add_char.attr_handle);

        /* Assign handles sequentially - characteristics are added in order */
        static int char_count = 0;
        char_count++;

        switch (char_count) {
            case 1: /* Button 1 Map */
                s_btn1_map_handle = param->add_char.attr_handle;
                config_storage_load_button(BUTTON_ID_2, &s_btn2_map[0], &s_btn2_map[1]);
                add_characteristic(s_service_handle, &s_btn2_map_handle,
                                   GATTS_CHAR_BTN2_MAP_UUID, s_char_property,
                                   s_btn2_map, BTN_MAP_LEN, NULL);
                break;
            case 2: /* Button 2 Map */
                s_btn2_map_handle = param->add_char.attr_handle;
                config_storage_load_button(BUTTON_ID_3, &s_btn3_map[0], &s_btn3_map[1]);
                add_characteristic(s_service_handle, &s_btn3_map_handle,
                                   GATTS_CHAR_BTN3_MAP_UUID, s_char_property,
                                   s_btn3_map, BTN_MAP_LEN, NULL);
                break;
            case 3: /* Button 3 Map */
                s_btn3_map_handle = param->add_char.attr_handle;
                config_storage_load_button(BUTTON_ID_4, &s_btn4_map[0], &s_btn4_map[1]);
                add_characteristic(s_service_handle, &s_btn4_map_handle,
                                   GATTS_CHAR_BTN4_MAP_UUID, s_char_property,
                                   s_btn4_map, BTN_MAP_LEN, NULL);
                break;
            case 4: /* Button 4 Map */
                s_btn4_map_handle = param->add_char.attr_handle;
                add_characteristic(s_service_handle, &s_btn_event_handle,
                                   GATTS_CHAR_BTN_EVENT_UUID, s_char_property,
                                   s_btn_event, BTN_EVENT_CHAR_LEN, NULL);
                break;
            case 5: /* Button Event - add CCCD for it */
                s_btn_event_handle = param->add_char.attr_handle;
                {
                esp_bt_uuid_t descr_uuid;
                descr_uuid.len = ESP_UUID_LEN_16;
                descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
                esp_err_t ret = esp_ble_gatts_add_char_descr(s_service_handle, &descr_uuid,
                                                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                             NULL, NULL);
                if (ret) {
                    ESP_LOGE(TAG, "add btn_event CCCD failed, error code = 0x%x", ret);
                }
                }
                break;
            case 6: /* Device Status - add CCCD for it */
                s_dev_status_handle = param->add_char.attr_handle;
                {
                esp_bt_uuid_t descr_uuid;
                descr_uuid.len = ESP_UUID_LEN_16;
                descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
                esp_err_t ret = esp_ble_gatts_add_char_descr(s_service_handle, &descr_uuid,
                                                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                             NULL, NULL);
                if (ret) {
                    ESP_LOGE(TAG, "add dev_status CCCD failed, error code = 0x%x", ret);
                }
                }
                break;
            case 7: /* TX Power - add Sleep Mode characteristic */
                s_tx_power_handle = param->add_char.attr_handle;
                add_characteristic(s_service_handle, &s_sleep_mode_handle,
                                   GATTS_CHAR_SLEEP_MODE_UUID, s_char_property,
                                   &s_sleep_mode, 1, NULL);
                break;
            case 8: /* Sleep Mode - all characteristics done, start service */
                s_sleep_mode_handle = param->add_char.attr_handle;
                {
                esp_err_t ret = esp_ble_gatts_start_service(s_service_handle);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "start service failed: 0x%x", ret);
                } else {
                    ESP_LOGI(TAG, "Service 0x1820 started");
                }
                }
                break;
        }
        break;
    }

    case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
        ESP_LOGI(TAG, "ADD_DESCR_EVT, status %d, attr_handle %d",
                 param->add_char_descr.status, param->add_char_descr.attr_handle);
        if (s_btn_event_descr_handle == 0) {
            /* Button Event CCCD handle received, now add Device Status characteristic */
            s_btn_event_descr_handle = param->add_char_descr.attr_handle;
            add_characteristic(s_service_handle, &s_dev_status_handle,
                               GATTS_CHAR_DEV_STATUS_UUID, s_char_property,
                               s_dev_status, DEV_STATUS_CHAR_LEN, NULL);
        } else if (s_dev_status_descr_handle == 0) {
            /* Device Status CCCD handle received.
             * Now add TX Power characteristic. */
            s_dev_status_descr_handle = param->add_char_descr.attr_handle;
            add_characteristic(s_service_handle, &s_tx_power_handle,
                               GATTS_CHAR_TX_POWER_UUID, s_char_property,
                               &s_tx_power, 1, NULL);
        }
        break;
    }

    case ESP_GATTS_START_EVT: {
        ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d",
                 param->start.status, param->start.service_handle);
        break;
    }

    case ESP_GATTS_READ_EVT: {
        ESP_LOGI(TAG, "GATT_READ_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d",
                 param->read.conn_id, param->read.trans_id, param->read.handle);

        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;

        if (param->read.handle == s_btn1_map_handle) {
            rsp.attr_value.len = BTN_MAP_LEN;
            memcpy(rsp.attr_value.value, s_btn1_map, BTN_MAP_LEN);
        } else if (param->read.handle == s_btn2_map_handle) {
            rsp.attr_value.len = BTN_MAP_LEN;
            memcpy(rsp.attr_value.value, s_btn2_map, BTN_MAP_LEN);
        } else if (param->read.handle == s_btn3_map_handle) {
            rsp.attr_value.len = BTN_MAP_LEN;
            memcpy(rsp.attr_value.value, s_btn3_map, BTN_MAP_LEN);
        } else if (param->read.handle == s_btn4_map_handle) {
            rsp.attr_value.len = BTN_MAP_LEN;
            memcpy(rsp.attr_value.value, s_btn4_map, BTN_MAP_LEN);
        } else if (param->read.handle == s_btn_event_handle) {
            rsp.attr_value.len = BTN_EVENT_CHAR_LEN;
            memcpy(rsp.attr_value.value, s_btn_event, BTN_EVENT_CHAR_LEN);
        } else if (param->read.handle == s_dev_status_handle) {
            rsp.attr_value.len = DEV_STATUS_CHAR_LEN;
            memcpy(rsp.attr_value.value, s_dev_status, DEV_STATUS_CHAR_LEN);
        } else if (param->read.handle == s_tx_power_handle) {
            rsp.attr_value.len = 1;
            rsp.attr_value.value[0] = s_tx_power;
        } else if (param->read.handle == s_sleep_mode_handle) {
            rsp.attr_value.len = 1;
            rsp.attr_value.value[0] = s_sleep_mode;
        }

        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        break;
    }

    case ESP_GATTS_WRITE_EVT: {
        ESP_LOGI(TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d",
                 param->write.conn_id, param->write.trans_id, param->write.handle);

        if (!param->write.is_prep) {
            ESP_LOGI(TAG, "GATT_WRITE_EVT, value len %d", param->write.len);
            ESP_LOG_BUFFER_HEX(TAG, param->write.value, param->write.len);

            /* Handle Button 1 Map write */
            if (param->write.handle == s_btn1_map_handle && param->write.len >= BTN_MAP_LEN) {
                s_btn1_map[0] = param->write.value[0];  /* vk_code */
                s_btn1_map[1] = param->write.value[1];  /* modifier */
                config_storage_save_button(BUTTON_ID_1, s_btn1_map[0], s_btn1_map[1]);
                ESP_LOGI(TAG, "Button 1 mapped to VK=0x%02X, MOD=0x%02X", s_btn1_map[0], s_btn1_map[1]);
            }
            /* Handle Button 2 Map write */
            else if (param->write.handle == s_btn2_map_handle && param->write.len >= BTN_MAP_LEN) {
                s_btn2_map[0] = param->write.value[0];
                s_btn2_map[1] = param->write.value[1];
                config_storage_save_button(BUTTON_ID_2, s_btn2_map[0], s_btn2_map[1]);
                ESP_LOGI(TAG, "Button 2 mapped to VK=0x%02X, MOD=0x%02X", s_btn2_map[0], s_btn2_map[1]);
            }
            /* Handle Button 3 Map write */
            else if (param->write.handle == s_btn3_map_handle && param->write.len >= BTN_MAP_LEN) {
                s_btn3_map[0] = param->write.value[0];
                s_btn3_map[1] = param->write.value[1];
                config_storage_save_button(BUTTON_ID_3, s_btn3_map[0], s_btn3_map[1]);
                ESP_LOGI(TAG, "Button 3 mapped to VK=0x%02X, MOD=0x%02X", s_btn3_map[0], s_btn3_map[1]);
            }
            /* Handle Button 4 Map write */
            else if (param->write.handle == s_btn4_map_handle && param->write.len >= BTN_MAP_LEN) {
                s_btn4_map[0] = param->write.value[0];
                s_btn4_map[1] = param->write.value[1];
                config_storage_save_button(BUTTON_ID_4, s_btn4_map[0], s_btn4_map[1]);
                ESP_LOGI(TAG, "Button 4 mapped to VK=0x%02X, MOD=0x%02X", s_btn4_map[0], s_btn4_map[1]);
            }
            /* Handle TX Power write (1 byte, 0-7) */
            else if (param->write.handle == s_tx_power_handle && param->write.len >= 1) {
                uint8_t level = param->write.value[0];
                if (level <= 7) {
                    s_tx_power = level;
                    config_storage_save_tx_power(level);
                    esp_bredr_tx_power_set((esp_power_level_t)level, (esp_power_level_t)level);
                    ESP_LOGI(TAG, "TX power set to level %d", level);
                }
            }
            /* Handle Sleep Mode write (1 byte, 0 or 1) */
            else if (param->write.handle == s_sleep_mode_handle && param->write.len >= 1) {
                uint8_t enabled = param->write.value[0] ? 1 : 0;
                s_sleep_mode = enabled;
                config_storage_save_sleep_mode(enabled);
                if (enabled) {
                    esp_bt_sleep_enable();
                } else {
                    esp_bt_sleep_disable();
                }
                ESP_LOGI(TAG, "Sleep mode %s", enabled ? "enabled" : "disabled");
            }
            /* Handle CCCD write (notification enable/disable) for Device Status */
            else if ((param->write.handle == s_dev_status_descr_handle) && param->write.len == 2) {
                uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
                if (descr_value == 0x0001) {
                    ESP_LOGI(TAG, "Device Status notify enable");
                } else if (descr_value == 0x0000) {
                    ESP_LOGI(TAG, "Device Status notify disable");
                }
            }
            /* Handle CCCD write for Button Event */
            else if ((param->write.handle == s_btn_event_descr_handle) && param->write.len == 2) {
                uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
                if (descr_value == 0x0001) {
                    ESP_LOGI(TAG, "Button Event notify enable");
                } else if (descr_value == 0x0000) {
                    ESP_LOGI(TAG, "Button Event notify disable");
                }
            }
        }

        prepare_write_event_env(gatts_if, &s_prepare_write_env, param);
        break;
    }

    case ESP_GATTS_EXEC_WRITE_EVT: {
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        exec_write_event_env(&s_prepare_write_env, param);
        break;
    }

    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "MTU exchange, MTU = %d", param->mtu.mtu);
        break;

    case ESP_GATTS_CONNECT_EVT: {
        /* Only handle events for our custom GATT app (not HID app) */
        if (gatts_if != s_gatts_if) break;

        ESP_LOGI(TAG, "BLE client connected, conn_id %d", param->connect.conn_id);
        s_conn_id = param->connect.conn_id;
        s_ble_connected = true;
        /* Request fast connection interval for low-latency keyboard events */
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        conn_params.min_int = 12;
        conn_params.max_int = 24;
        conn_params.latency = 0;
        conn_params.timeout = 500;
        esp_ble_gap_update_conn_params(&conn_params);

        /* Keep advertising so the Python config app can also connect */
        esp_ble_gap_start_advertising(&adv_params);

        if (!bt_hfp_is_connected()) {
            esp_bd_addr_t saved_addr = {0};
            if (config_storage_load_hfp_addr(saved_addr) == ESP_OK) {
                ESP_LOGI(TAG, "Triggering HFP reconnect to %02x:%02x:%02x:%02x:%02x:%02x",
                         saved_addr[0], saved_addr[1], saved_addr[2],
                         saved_addr[3], saved_addr[4], saved_addr[5]);
                esp_hf_client_connect(saved_addr);
            }
        }
        break;
    }

    case ESP_GATTS_DISCONNECT_EVT: {
        if (gatts_if != s_gatts_if) break;

        ESP_LOGI(TAG, "BLE client disconnected, retry advertising in 5s");
        s_ble_connected = false;
        s_conn_id = 0;
        /* Delayed retry — avoids BTDM security manager conflicts
         * if Classic BT is mid-pairing when BLE disconnects. */
        TimerHandle_t t = xTimerCreate("adv_retry", pdMS_TO_TICKS(5000),
                                       pdFALSE, NULL, adv_retry_cb);
        if (t) xTimerStart(t, 0);
        break;
    }

    case ESP_GATTS_CONF_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT status %d", param->conf.status);
        break;

    default:
        break;
    }
}

void ble_gatts_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE GATT server");

    esp_err_t ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret) {
        ESP_LOGE(TAG, "gatts register error, error code = 0x%x", ret);
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(TAG, "gap register error, error code = 0x%x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(GATTS_APP_ID);
    if (ret) {
        ESP_LOGE(TAG, "gatts app register error, error code = 0x%x", ret);
        return;
    }

    ret = esp_ble_gatt_set_local_mtu(500);
    if (ret) {
        ESP_LOGE(TAG, "set local MTU failed, error code = 0x%x", ret);
    }

    ESP_LOGI(TAG, "BLE GATT server initialized");
}

void ble_send_button_event(uint8_t button_id, uint8_t state)
{
    if (s_gatts_if == ESP_GATT_IF_NONE || !s_ble_connected) return;

    uint8_t data[BTN_EVENT_CHAR_LEN];
    data[0] = button_id;
    data[1] = state;

    esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_btn_event_handle,
                                sizeof(data), data, false);
}

void ble_send_device_status(uint8_t hfp_connected, uint8_t audio_active)
{
    if (s_gatts_if == ESP_GATT_IF_NONE || !s_ble_connected) return;

    uint8_t data[DEV_STATUS_CHAR_LEN];
    data[0] = hfp_connected;
    data[1] = audio_active;

    esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_dev_status_handle,
                                sizeof(data), data, false);
}

void ble_get_button_mapping(uint8_t button_id, uint8_t *vk_code, uint8_t *modifier)
{
    switch (button_id) {
    case BUTTON_ID_1:
        *vk_code = s_btn1_map[0];
        *modifier = s_btn1_map[1];
        break;
    case BUTTON_ID_2:
        *vk_code = s_btn2_map[0];
        *modifier = s_btn2_map[1];
        break;
    case BUTTON_ID_3:
        *vk_code = s_btn3_map[0];
        *modifier = s_btn3_map[1];
        break;
    case BUTTON_ID_4:
        *vk_code = s_btn4_map[0];
        *modifier = s_btn4_map[1];
        break;
    default:
        *vk_code = 0;
        *modifier = 0;
        break;
    }
}

uint8_t ble_get_button_vk(uint8_t button_id)
{
    uint8_t vk = 0, mod = 0;
    ble_get_button_mapping(button_id, &vk, &mod);
    return vk;
}

uint8_t ble_get_button_mod(uint8_t button_id)
{
    uint8_t vk = 0, mod = 0;
    ble_get_button_mapping(button_id, &vk, &mod);
    return mod;
}

void ble_gatts_adv_stop(void)
{
    esp_ble_gap_stop_advertising();
    ESP_LOGI(TAG, "BLE advertising stopped (SCO active)");
}

void ble_gatts_adv_start(void)
{
    esp_ble_gap_start_advertising(&adv_params);
    ESP_LOGI(TAG, "BLE advertising restarted");
}

bool ble_gatts_is_connected(void)
{
    return s_ble_connected;
}
