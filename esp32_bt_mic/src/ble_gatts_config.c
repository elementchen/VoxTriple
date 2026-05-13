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
#include "esp_gatt_common_api.h"
#include "ble_gatts_config.h"
#include "config_storage.h"

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

#define GATTS_NUM_HANDLES    16
#define GATTS_APP_ID         0x01
#define PREPARE_BUF_MAX_SIZE 1024

#define BLE_ADV_NAME          "ESP32_BT_MIC"

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
static uint16_t s_btn_event_handle = 0;
static uint16_t s_dev_status_handle = 0;
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
static uint8_t s_btn_event[BTN_EVENT_CHAR_LEN] = {0, 0};
static uint8_t s_dev_status[DEV_STATUS_CHAR_LEN] = {0, 0};

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
    int name_len = strlen(name);
    /* BLE advertising data max 31 bytes.
     * AD1: Flags (3 bytes)
     * AD2: Short name (2 + name_len bytes) — fits within 31-byte limit
     * Service UUID goes in scan response data. */
    uint8_t raw_adv_data[3 + 2 + name_len];
    int pos = 0;

    /* Flags: General Discoverable, dual-mode (no BREDR_NOT_SPT) */
    raw_adv_data[pos++] = 2;
    raw_adv_data[pos++] = ESP_BLE_AD_TYPE_FLAG;
    raw_adv_data[pos++] = ESP_BLE_ADV_FLAG_GEN_DISC;

    /* Shortened Local Name */
    raw_adv_data[pos++] = name_len + 1;
    raw_adv_data[pos++] = ESP_BLE_AD_TYPE_NAME_SHORT;
    memcpy(&raw_adv_data[pos], name, name_len);
    pos += name_len;

    esp_err_t ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
    if (ret) {
        ESP_LOGE(TAG, "config raw adv data failed, error code = 0x%x", ret);
    }

    /* Scan response: 128-bit service UUID for discoverability */
    uint8_t scan_rsp[18];
    scan_rsp[0] = 17;
    scan_rsp[1] = ESP_BLE_AD_TYPE_128SRV_CMPL;
    uint8_t uuid128[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
                           0x00, 0x10, 0x00, 0x00, 0x20, 0x18, 0x00, 0x00};
    memcpy(&scan_rsp[2], uuid128, 16);

    ret = esp_ble_gap_config_scan_rsp_data_raw(scan_rsp, sizeof(scan_rsp));
    if (ret) {
        ESP_LOGE(TAG, "config raw scan rsp data failed, error code = 0x%x", ret);
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
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
        ble_init_adv_data(BLE_ADV_NAME);

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
                add_characteristic(s_service_handle, &s_btn_event_handle,
                                   GATTS_CHAR_BTN_EVENT_UUID, s_char_property,
                                   s_btn_event, BTN_EVENT_CHAR_LEN, NULL);
                break;
            case 4: /* Button Event - add CCCD for it */
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
            case 5: /* Device Status - add CCCD for it */
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
            /* Device Status CCCD handle received — all chars + descs done.
             * Now START the service so it becomes visible to BLE clients. */
            s_dev_status_descr_handle = param->add_char_descr.attr_handle;
            esp_err_t ret = esp_ble_gatts_start_service(s_service_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "start service failed: 0x%x", ret);
            } else {
                ESP_LOGI(TAG, "Service 0x1820 started");
            }
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
        } else if (param->read.handle == s_btn_event_handle) {
            rsp.attr_value.len = BTN_EVENT_CHAR_LEN;
            memcpy(rsp.attr_value.value, s_btn_event, BTN_EVENT_CHAR_LEN);
        } else if (param->read.handle == s_dev_status_handle) {
            rsp.attr_value.len = DEV_STATUS_CHAR_LEN;
            memcpy(rsp.attr_value.value, s_dev_status, DEV_STATUS_CHAR_LEN);
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
        ESP_LOGI(TAG, "BLE connected, conn_id %d", param->connect.conn_id);
        s_conn_id = param->connect.conn_id;
        s_ble_connected = true;
        /* Request fast connection interval for low-latency keyboard events */
        esp_ble_conn_update_params_t conn_params = {
            .min_int = 12,     /* 15ms — fast for first press */
            .max_int = 24,     /* 30ms */
            .latency = 0,      /* no slave latency */
            .timeout = 500,    /* 5s supervision timeout */
        };
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    }

    case ESP_GATTS_DISCONNECT_EVT: {
        ESP_LOGI(TAG, "BLE disconnected, restarting advertising");
        s_ble_connected = false;
        s_conn_id = 0;
        esp_ble_gap_start_advertising(&adv_params);
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
    default:
        *vk_code = 0;
        *modifier = 0;
        break;
    }
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
