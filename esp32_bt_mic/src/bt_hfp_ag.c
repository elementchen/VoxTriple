/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_ag_api.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"
#include "sdkconfig.h"
#include "bt_app_core.h"
#include "bt_app_hf.h"
#include "bt_init.h"
#include "audio_capture.h"
#include "osi/allocator.h"

static const char *TAG = "BT_HFP_AG";

/* HFP event strings */
static const char *c_hf_evt_str[] = {
    "CONNECTION_STATE_EVT",
    "AUDIO_STATE_EVT",
    "VR_STATE_CHANGE_EVT",
    "VOLUME_CONTROL_EVT",
    "UNKNOW_AT_CMD",
    "IND_UPDATE",
    "CIND_RESPONSE_EVT",
    "COPS_RESPONSE_EVT",
    "CLCC_RESPONSE_EVT",
    "CNUM_RESPONSE_EVT",
    "DTMF_RESPONSE_EVT",
    "NREC_RESPONSE_EVT",
    "ANSWER_INCOMING_EVT",
    "REJECT_INCOMING_EVT",
    "DIAL_EVT",
    "WBS_EVT",
    "BCS_EVT",
    "PKT_STAT_EVT",
    "PROF_STATE_EVT",
};

static const char *c_connection_state_str[] = {
    "DISCONNECTED",
    "CONNECTING",
    "CONNECTED",
    "SLC_CONNECTED",
    "DISCONNECTING",
};

static const char *c_audio_state_str[] = {
    "disconnected",
    "connecting",
    "connected",
    "connected_msbc",
};

static const char *c_codec_mode_str[] = {
    "CVSD Only",
    "Use CVSD",
    "Use MSBC",
};

#if CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI
/*
 * HFP audio timing constants:
 * 7500 us = 12 slots, aligned to 1 mSBC frame, multiple of common Tesco
 */
#define PCM_BLOCK_DURATION_US       (7500)
#define WBS_PCM_SAMPLING_RATE_KHZ   (16)
#define PCM_SAMPLING_RATE_KHZ       (8)
#define BYTES_PER_SAMPLE            (2)
#define WBS_PCM_INPUT_DATA_SIZE     (WBS_PCM_SAMPLING_RATE_KHZ * PCM_BLOCK_DURATION_US / 1000 * BYTES_PER_SAMPLE)
#define PCM_INPUT_DATA_SIZE         (PCM_SAMPLING_RATE_KHZ * PCM_BLOCK_DURATION_US / 1000 * BYTES_PER_SAMPLE)
#define ESP_HFP_RINGBUF_SIZE        4800
#define PCM_GENERATOR_TICK_US       (4000)

static RingbufHandle_t s_m_rb = NULL;
static SemaphoreHandle_t s_send_data_sem = NULL;
static TaskHandle_t s_bt_send_task_handle = NULL;
static esp_hf_audio_state_t s_audio_codec = ESP_HF_AUDIO_STATE_DISCONNECTED;
static uint64_t s_last_enter_time = 0;
static uint64_t s_data_num = 0;
static uint64_t s_time_old = 0;
static esp_timer_handle_t s_periodic_timer = NULL;

/**
 * @brief Outgoing data callback - reads PCM from ring buffer to send via SCO
 */
static uint32_t bt_app_hf_outgoing_cb(uint8_t *p_buf, uint32_t sz)
{
    size_t item_size = 0;
    uint8_t *data = NULL;

    if (!s_m_rb) {
        return 0;
    }

    vRingbufferGetInfo(s_m_rb, NULL, NULL, NULL, NULL, &item_size);
    if (item_size >= sz) {
        data = xRingbufferReceiveUpTo(s_m_rb, &item_size, 0, sz);
        if (data) {
            memcpy(p_buf, data, item_size);
            vRingbufferReturnItem(s_m_rb, data);
            return sz;
        }
    }
    return 0;
}

/**
 * @brief Incoming data callback - receives audio from SCO (not used for mic project)
 */
static void bt_app_hf_incoming_cb(const uint8_t *buf, uint32_t sz)
{
    uint64_t time_new = esp_timer_get_time();
    s_data_num += sz;
    if ((time_new - s_time_old) >= 3000000) {
        float tick_s = (time_new - s_time_old) / 1000000.0;
        float speed = s_data_num * 8 / tick_s / 1000.0;
        ESP_LOGI(TAG, "HFP audio speed: %.2f kbit/s", speed);
        s_data_num = 0;
        s_time_old = time_new;
    }
}

/**
 * @brief Timer callback to trigger data send at regular intervals
 */
static void bt_app_send_data_timer_cb(void *arg)
{
    if (!xSemaphoreGive(s_send_data_sem)) {
        ESP_LOGE(TAG, "%s xSemaphoreGive failed", __func__);
    }
}

/**
 * @brief Task to calculate and supply correct amount of PCM data per time slot
 */
static void bt_app_send_data_task(void *arg)
{
    uint64_t frame_data_num;
    size_t item_size = 0;
    uint8_t *buf = NULL;

    for (;;) {
        if (xSemaphoreTake(s_send_data_sem, (TickType_t)portMAX_DELAY)) {
            uint64_t now = esp_timer_get_time();
            uint64_t duration = now - s_last_enter_time;

            if (s_audio_codec == ESP_HF_AUDIO_STATE_CONNECTED_MSBC) {
                frame_data_num = duration / PCM_BLOCK_DURATION_US * WBS_PCM_INPUT_DATA_SIZE;
                s_last_enter_time += frame_data_num / WBS_PCM_INPUT_DATA_SIZE * PCM_BLOCK_DURATION_US;
            } else {
                frame_data_num = duration / PCM_BLOCK_DURATION_US * PCM_INPUT_DATA_SIZE;
                s_last_enter_time += frame_data_num / PCM_INPUT_DATA_SIZE * PCM_BLOCK_DURATION_US;
            }

            if (frame_data_num == 0) {
                continue;
            }

            buf = osi_malloc(frame_data_num);
            if (!buf) {
                ESP_LOGE(TAG, "%s, no mem", __func__);
                continue;
            }

            /* Read audio from I2S via audio_capture and fill the buffer */
            size_t total_read = 0;
            while (total_read < frame_data_num) {
                size_t bytes_read = 0;
                esp_err_t ret = audio_capture_read(buf + total_read,
                                                   frame_data_num - total_read,
                                                   &bytes_read, 100);
                if (ret != ESP_OK || bytes_read == 0) {
                    /* If mic data not available, fill with silence */
                    memset(buf + total_read, 0, frame_data_num - total_read);
                    total_read = frame_data_num;
                    break;
                }
                total_read += bytes_read;
            }

            BaseType_t done = xRingbufferSend(s_m_rb, buf, frame_data_num, 0);
            if (!done) {
                ESP_LOGE(TAG, "rb send fail");
            }
            osi_free(buf);

            vRingbufferGetInfo(s_m_rb, NULL, NULL, NULL, NULL, &item_size);

            if (s_audio_codec == ESP_HF_AUDIO_STATE_CONNECTED_MSBC) {
                if (item_size >= WBS_PCM_INPUT_DATA_SIZE) {
                    esp_hf_ag_outgoing_data_ready();
                }
            } else {
                if (item_size >= PCM_INPUT_DATA_SIZE) {
                    esp_hf_ag_outgoing_data_ready();
                }
            }
        }
    }
}

void bt_hfp_audio_start(void)
{
    if (s_send_data_sem) {
        ESP_LOGW(TAG, "Audio already started");
        return;
    }

    ESP_LOGI(TAG, "Starting HFP audio streaming");

    /* Create ring buffer */
    s_m_rb = xRingbufferCreate(ESP_HFP_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    assert(s_m_rb);

    /* Create semaphore */
    s_send_data_sem = xSemaphoreCreateBinary();

    /* Create send task */
    xTaskCreate(bt_app_send_data_task, "BtSendData", 4096, NULL,
                configMAX_PRIORITIES - 3, &s_bt_send_task_handle);

    /* Create periodic timer to pace data output */
    const esp_timer_create_args_t timer_args = {
        .callback = &bt_app_send_data_timer_cb,
        .name = "periodic"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_periodic_timer, PCM_GENERATOR_TICK_US));

    s_last_enter_time = esp_timer_get_time();
    s_time_old = s_last_enter_time;

    /* Start audio capture from I2S microphone (bt_app_send_data_task reads from it) */
    audio_capture_start();
}

void bt_hfp_audio_stop(void)
{
    ESP_LOGI(TAG, "Stopping HFP audio streaming");

    /* Stop audio capture */
    audio_capture_stop();

    /* Stop timer */
    if (s_periodic_timer) {
        esp_timer_stop(s_periodic_timer);
        esp_timer_delete(s_periodic_timer);
        s_periodic_timer = NULL;
    }

    /* Delete send task */
    if (s_bt_send_task_handle) {
        vTaskDelete(s_bt_send_task_handle);
        s_bt_send_task_handle = NULL;
    }

    /* Delete semaphore */
    if (s_send_data_sem) {
        vSemaphoreDelete(s_send_data_sem);
        s_send_data_sem = NULL;
    }

    /* Delete ring buffer */
    if (s_m_rb) {
        vRingbufferDelete(s_m_rb);
        s_m_rb = NULL;
    }
}

void bt_hfp_feed_audio(const uint8_t *data, uint32_t len)
{
    if (s_m_rb) {
        xRingbufferSend(s_m_rb, data, len, 0);
    }
}
#endif /* CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI */

void bt_app_hf_cb(esp_hf_cb_event_t event, esp_hf_cb_param_t *param)
{
    if (event <= ESP_HF_PROF_STATE_EVT) {
        ESP_LOGI(TAG, "HFP AG event: %s", c_hf_evt_str[event]);
    } else {
        ESP_LOGE(TAG, "HFP AG invalid event %d", event);
    }

    switch (event) {
    case ESP_HF_CONNECTION_STATE_EVT: {
        ESP_LOGI(TAG, "connection state %s, peer feats 0x%" PRIx32 ", chld_feats 0x%" PRIx32,
                 c_connection_state_str[param->conn_stat.state],
                 param->conn_stat.peer_feat,
                 param->conn_stat.chld_feat);

        memcpy(hf_peer_addr, param->conn_stat.remote_bda, ESP_BD_ADDR_LEN);

        bool connected = (param->conn_stat.state == ESP_HF_CONNECTION_STATE_SLC_CONNECTED);
        bt_hfp_set_connected(connected);

        /* Notify BLE about status change */
        extern void ble_send_device_status(uint8_t hfp_connected, uint8_t audio_active);
        ble_send_device_status(bt_hfp_is_connected(), bt_audio_is_active());
        break;
    }

    case ESP_HF_AUDIO_STATE_EVT: {
        ESP_LOGI(TAG, "Audio State %s", c_audio_state_str[param->audio_stat.state]);

#if CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI
        if (param->audio_stat.state == ESP_HF_AUDIO_STATE_CONNECTED ||
            param->audio_stat.state == ESP_HF_AUDIO_STATE_CONNECTED_MSBC) {

            if (param->audio_stat.state == ESP_HF_AUDIO_STATE_CONNECTED) {
                s_audio_codec = ESP_HF_AUDIO_STATE_CONNECTED;
            } else {
                s_audio_codec = ESP_HF_AUDIO_STATE_CONNECTED_MSBC;
            }

            esp_hf_ag_register_data_callback(bt_app_hf_incoming_cb, bt_app_hf_outgoing_cb);

            bt_audio_set_active(true);
            bt_hfp_audio_start();

            /* Notify BLE */
            extern void ble_send_device_status(uint8_t hfp_connected, uint8_t audio_active);
            ble_send_device_status(bt_hfp_is_connected(), bt_audio_is_active());

        } else if (param->audio_stat.state == ESP_HF_AUDIO_STATE_DISCONNECTED) {
            ESP_LOGI(TAG, "Audio connection disconnected");
            bt_audio_set_active(false);
            bt_hfp_audio_stop();

            /* Notify BLE */
            extern void ble_send_device_status(uint8_t hfp_connected, uint8_t audio_active);
            ble_send_device_status(bt_hfp_is_connected(), bt_audio_is_active());
        }
#endif
        break;
    }

    case ESP_HF_BVRA_RESPONSE_EVT: {
        ESP_LOGI(TAG, "Voice Recognition event");
        break;
    }

    case ESP_HF_VOLUME_CONTROL_EVT: {
        ESP_LOGI(TAG, "Volume Target: %d, Volume %d",
                 param->volume_control.type, param->volume_control.volume);
        break;
    }

    case ESP_HF_UNAT_RESPONSE_EVT: {
        ESP_LOGI(TAG, "Unknown AT CMD: %s", param->unat_rep.unat);
        esp_hf_ag_unknown_at_send(param->unat_rep.remote_addr, NULL);
        break;
    }

    case ESP_HF_IND_UPDATE_EVT: {
        ESP_LOGI(TAG, "UPDATE INDICATOR");
        esp_hf_ag_ciev_report(param->ind_upd.remote_addr, ESP_HF_IND_TYPE_CALL, 0);
        esp_hf_ag_ciev_report(param->ind_upd.remote_addr, ESP_HF_IND_TYPE_CALLSETUP, 0);
        esp_hf_ag_ciev_report(param->ind_upd.remote_addr, ESP_HF_IND_TYPE_SERVICE, 1);
        esp_hf_ag_ciev_report(param->ind_upd.remote_addr, ESP_HF_IND_TYPE_SIGNAL, 5);
        esp_hf_ag_ciev_report(param->ind_upd.remote_addr, ESP_HF_IND_TYPE_BATTCHG, 5);
        break;
    }

    case ESP_HF_CIND_RESPONSE_EVT: {
        ESP_LOGI(TAG, "CIND Response");
        esp_hf_ag_cind_response(param->cind_rep.remote_addr, 0, 0, 1, 5, 0, 5, 0);
        break;
    }

    case ESP_HF_COPS_RESPONSE_EVT: {
        esp_hf_ag_cops_response(param->cops_rep.remote_addr, "ESP32_MIC");
        break;
    }

    case ESP_HF_CLCC_RESPONSE_EVT: {
        int index = 1;
        esp_hf_current_call_direction_t dir = 1;
        esp_hf_current_call_status_t current_call_status = 0;
        esp_hf_current_call_mode_t mode = 0;
        esp_hf_current_call_mpty_type_t mpty = 0;
        char *number = {"123456"};
        esp_hf_call_addr_type_t type = ESP_HF_CALL_ADDR_TYPE_UNKNOWN;

        ESP_LOGI(TAG, "Calling Line Identification");
        esp_hf_ag_clcc_response(param->clcc_rep.remote_addr, index, dir,
                                current_call_status, mode, mpty, number, type);

        /* Send OK (index=0 means response OK) */
        index = 0;
        esp_hf_ag_clcc_response(param->clcc_rep.remote_addr, index, dir,
                                current_call_status, mode, mpty, number, type);
        break;
    }

    case ESP_HF_CNUM_RESPONSE_EVT: {
        char *number = {"123456"};
        int number_type = 129;
        esp_hf_subscriber_service_type_t service_type = ESP_HF_SUBSCRIBER_SERVICE_TYPE_VOICE;
        esp_hf_ag_cnum_response(hf_peer_addr, number, number_type, service_type);
        break;
    }

    case ESP_HF_VTS_RESPONSE_EVT: {
        ESP_LOGI(TAG, "DTMF code: %s", param->vts_rep.code);
        break;
    }

    case ESP_HF_NREC_RESPONSE_EVT: {
        ESP_LOGI(TAG, "NREC state: %d", param->nrec.state);
        break;
    }

    case ESP_HF_ATA_RESPONSE_EVT: {
        ESP_LOGI(TAG, "Answer Incoming Call");
        esp_hf_ag_answer_call(param->ata_rep.remote_addr, 1, 0, 1, 0, "123456", 0);
        break;
    }

    case ESP_HF_CHUP_RESPONSE_EVT: {
        ESP_LOGI(TAG, "Reject Incoming Call");
        esp_hf_ag_reject_call(param->chup_rep.remote_addr, 0, 0, 0, 0, "123456", 0);
        break;
    }

    case ESP_HF_DIAL_EVT: {
        if (param->out_call.num_or_loc) {
            if (param->out_call.type == ESP_HF_DIAL_NUM) {
                ESP_LOGI(TAG, "Dial number \"%s\"", param->out_call.num_or_loc);
                esp_hf_ag_cmee_send(param->out_call.remote_addr, ESP_HF_AT_RESPONSE_CODE_OK, ESP_HF_CME_AG_FAILURE);
                esp_hf_ag_out_call(param->out_call.remote_addr, 1, 0, 1, 0, param->out_call.num_or_loc, 0);
            } else if (param->out_call.type == ESP_HF_DIAL_MEM) {
                ESP_LOGI(TAG, "Dial memory \"%s\"", param->out_call.num_or_loc);
                esp_hf_ag_cmee_send(param->out_call.remote_addr, ESP_HF_AT_RESPONSE_CODE_OK, ESP_HF_CME_AG_FAILURE);
                esp_hf_ag_out_call(param->out_call.remote_addr, 1, 0, 1, 0, "123456", 0);
            }
        } else {
            ESP_LOGI(TAG, "Dial last number");
        }
        break;
    }

#if (CONFIG_BT_HFP_WBS_ENABLE)
    case ESP_HF_WBS_RESPONSE_EVT: {
        ESP_LOGI(TAG, "Current codec: %s", c_codec_mode_str[param->wbs_rep.codec]);
        break;
    }
#endif

    case ESP_HF_BCS_RESPONSE_EVT: {
        ESP_LOGI(TAG, "Codec negotiation: %s", c_codec_mode_str[param->bcs_rep.mode]);
        break;
    }

    case ESP_HF_PKT_STAT_NUMS_GET_EVT: {
        ESP_LOGI(TAG, "PKT_STAT_NUMS_GET_EVT: %d", event);
        break;
    }

    case ESP_HF_PROF_STATE_EVT: {
        if (ESP_HF_INIT_SUCCESS == param->prof_stat.state) {
            ESP_LOGI(TAG, "AG PROF STATE: Init Complete");
        } else if (ESP_HF_DEINIT_SUCCESS == param->prof_stat.state) {
            ESP_LOGI(TAG, "AG PROF STATE: Deinit Complete");
        } else {
            ESP_LOGE(TAG, "AG PROF STATE error: %d", param->prof_stat.state);
        }
        break;
    }

    default:
        ESP_LOGI(TAG, "Unsupported HFP AG EVT: %d", event);
        break;
    }
}
