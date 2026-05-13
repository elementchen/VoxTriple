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
#include "esp_hf_client_api.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"
#include "sdkconfig.h"
#include "bt_app_core.h"
#include "bt_app_hf.h"
#include "bt_init.h"
#include "audio_capture.h"
#include "ble_gatts_config.h"
#include "osi/allocator.h"

static const char *TAG = "BT_HFP_HF";

/* Dispatched handlers — run in app task, not BTU_TASK (prototypes) */
static void audio_start_handler(uint16_t event, void *param);
static void audio_stop_handler(uint16_t event, void *param);

/* ----------------------------------------------------------------
 * Audio DSP: high-pass filter + noise gate for cleaner voice input
 * All filters use int64_t intermediates to avoid overflow on ESP32.
 * ---------------------------------------------------------------- */

/* HPF: 1-pole IIR, 80Hz cutoff @ 16kHz (removes rumble/wind) */
static int16_t s_hpf_prev_in = 0;
static int64_t s_hpf_prev_out = 0;
#define HPF_ALPHA_Q15  31775

/* Moving-average ring (5-tap, ~312µs window). Smooths high-frequency hiss. */
#define MA_TAPS         5
static int16_t s_ma_ring[MA_TAPS] = {0};
static int32_t s_ma_sum = 0;
static uint8_t s_ma_idx = 0;
static uint8_t s_ma_fill = 0;

/* Noise gate (disabled by default; uncomment call in audio_dsp_process) */
#define GATE_THRESHOLD   1638
#define GATE_HOLD_COUNT    18
static int s_gate_hold = 0;
static bool s_gate_open = true;
static int64_t s_env = 0;
#define ENV_ATTACK_Q15   16384
#define ENV_RELEASE_Q15    655

static void apply_hpf(uint8_t *buf, size_t len)
{
    int16_t *samples = (int16_t *)buf;
    size_t count = len >> 1;
    for (size_t i = 0; i < count; i++) {
        int64_t x = samples[i];
        int64_t accum = s_hpf_prev_out + x - s_hpf_prev_in;
        int64_t y = (HPF_ALPHA_Q15 * accum) >> 15;
        s_hpf_prev_in = (int16_t)x;
        s_hpf_prev_out = y;
        if (y > 32767) y = 32767;
        else if (y < -32768) y = -32768;
        samples[i] = (int16_t)y;
    }
}

/* 5-tap moving-average: low-pass that smooths digital hiss/spikes */
static void apply_moving_average(uint8_t *buf, size_t len)
{
    int16_t *samples = (int16_t *)buf;
    size_t count = len >> 1;
    for (size_t i = 0; i < count; i++) {
        s_ma_sum -= s_ma_ring[s_ma_idx];
        s_ma_ring[s_ma_idx] = samples[i];
        s_ma_sum += samples[i];
        s_ma_idx = (s_ma_idx + 1) % MA_TAPS;
        if (s_ma_fill < MA_TAPS) s_ma_fill++;
        samples[i] = (int16_t)(s_ma_sum / s_ma_fill);
    }
}

static void apply_noise_gate(uint8_t *buf, size_t len)
{
    int16_t *samples = (int16_t *)buf;
    size_t count = len >> 1;
    for (size_t i = 0; i < count; i++) {
        int64_t abs_sample = (samples[i] >= 0) ? samples[i] : -samples[i];
        if (abs_sample > s_env)
            s_env += ((abs_sample - s_env) * (int64_t)ENV_ATTACK_Q15) >> 15;
        else
            s_env -= ((s_env - abs_sample) * (int64_t)ENV_RELEASE_Q15) >> 15;
        if (s_env < 0) s_env = 0;
        if (s_env >= GATE_THRESHOLD) {
            s_gate_open = true;
            s_gate_hold = GATE_HOLD_COUNT;
        } else if (s_gate_hold > 0) {
            s_gate_hold--;
        } else {
            s_gate_open = false;
        }
        if (!s_gate_open) samples[i] = 0;
    }
}

static void audio_dsp_process(uint8_t *buf, size_t len)
{
    apply_hpf(buf, len);               /* remove low-frequency rumble */
    apply_moving_average(buf, len);    /* smooth digital hiss/spikes */
    // apply_noise_gate(buf, len);     /* uncomment to enable gate */
}

#if CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI

/* HFP audio parameters */
#define ESP_HFP_RINGBUF_SIZE         9600
#define PCM_GENERATOR_TICK_US        (5000)
#define AUDIO_READ_CHUNK             (320)   /* 10ms @ 16kHz mono = 320 bytes */

static RingbufHandle_t s_m_rb = NULL;
static SemaphoreHandle_t s_send_data_sem = NULL;
static TaskHandle_t s_bt_send_task_handle = NULL;
static esp_hf_client_audio_state_t s_audio_codec = ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED;
static esp_timer_handle_t s_periodic_timer = NULL;
static volatile bool s_audio_running = false;
static volatile bool s_ptt_opened_audio = false;  /* track if PTT (not Windows) opened SCO */

/**
 * @brief Outgoing data callback.  Returns PCM data from ring buffer.
 *        On underflow returns silence (zeros) instead of 0 so the SCO
 *        link stays clean — no clicking/corruption artefacts.
 */
static uint32_t bt_app_hf_outgoing_cb(uint8_t *p_buf, uint32_t sz)
{
    size_t item_size = 0;

    if (!s_m_rb || !s_audio_running) {
        memset(p_buf, 0, sz);
        return sz;
    }

    vRingbufferGetInfo(s_m_rb, NULL, NULL, NULL, NULL, &item_size);
    if (item_size >= sz) {
        uint8_t *data = xRingbufferReceiveUpTo(s_m_rb, &item_size, 0, sz);
        if (data) {
            memcpy(p_buf, data, item_size);
            vRingbufferReturnItem(s_m_rb, data);
            return sz;
        }
    }
    /* Underflow: return silence. Keeps eSCO link clean. */
    memset(p_buf, 0, sz);
    return sz;
}

static void bt_app_hf_incoming_cb(const uint8_t *buf, uint32_t sz)
{
    /* Not used for microphone-only device */
}

static void bt_app_send_data_timer_cb(void *arg)
{
    xSemaphoreGive(s_send_data_sem);
}

/**
 * @brief Audio pump: reads I2S continuously, DSPs it, feeds ring buffer.
 *        No more frame-size batching — ring buffer decouples I2S rate
 *        from HFP mSBC frame rate naturally.
 */
static void bt_app_send_data_task(void *arg)
{
    uint8_t *buf = osi_malloc(AUDIO_READ_CHUNK);
    assert(buf);

    for (;;) {
        if (xSemaphoreTake(s_send_data_sem, (TickType_t)portMAX_DELAY)) {
            size_t bytes_read = 0;
            esp_err_t ret = audio_capture_read(buf, AUDIO_READ_CHUNK,
                                               &bytes_read, 50);
            /* Only process aligned reads so DSP gets whole int16 samples */
            if (ret == ESP_OK && bytes_read >= 2 && (bytes_read & 1) == 0) {
                audio_dsp_process(buf, bytes_read);
                xRingbufferSend(s_m_rb, buf, bytes_read, 0);
            }

            size_t item_size = 0;
            vRingbufferGetInfo(s_m_rb, NULL, NULL, NULL, NULL, &item_size);
            /* Signal ready when we have at least one mSBC frame (240B) */
            if (item_size >= 240) {
                esp_hf_client_outgoing_data_ready();
            }
        }
    }

    osi_free(buf);
    vTaskDelete(NULL);
}

void bt_hfp_hf_audio_start(void)
{
    if (s_send_data_sem) {
        ESP_LOGW(TAG, "Audio already started");
        return;
    }

    ESP_LOGI(TAG, "Starting HFP HF audio streaming");

    s_audio_running = true;

    s_m_rb = xRingbufferCreate(ESP_HFP_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    assert(s_m_rb);

    s_send_data_sem = xSemaphoreCreateBinary();

    xTaskCreate(bt_app_send_data_task, "BtSendData", 4096, NULL,
                5, &s_bt_send_task_handle);

    const esp_timer_create_args_t timer_args = {
        .callback = &bt_app_send_data_timer_cb,
        .name = "periodic"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_periodic_timer, PCM_GENERATOR_TICK_US));

    /* Pause BLE advertising to avoid BTDM scheduling conflicts with SCO */
    ble_gatts_adv_stop();

    audio_capture_start();
}

/* Stop data pipeline only (timer + flag). Resources cleaned up in audio_stop(). */
static void bt_hfp_hf_audio_pipeline_stop(void)
{
    s_audio_running = false;
    if (s_periodic_timer) {
        esp_timer_stop(s_periodic_timer);
        esp_timer_delete(s_periodic_timer);
        s_periodic_timer = NULL;
    }
}

void bt_hfp_hf_audio_stop(void)
{
    ESP_LOGI(TAG, "Stopping HFP HF audio streaming");

    bt_hfp_hf_audio_pipeline_stop();

    audio_capture_stop();

    /* Resume BLE advertising now that SCO is released */
    ble_gatts_adv_start();

    if (s_bt_send_task_handle) {
        vTaskDelete(s_bt_send_task_handle);
        s_bt_send_task_handle = NULL;
    }

    if (s_send_data_sem) {
        vSemaphoreDelete(s_send_data_sem);
        s_send_data_sem = NULL;
    }

    if (s_m_rb) {
        vRingbufferDelete(s_m_rb);
        s_m_rb = NULL;
    }
}

#endif /* CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI */

/* Dispatched handlers — called from app task via bt_app_work_dispatch */
static void audio_start_handler(uint16_t event, void *param)
{
    bt_hfp_hf_audio_start();
}

static void audio_stop_handler(uint16_t event, void *param)
{
    bt_hfp_hf_audio_stop();
}

/* PTT (Push-To-Talk) — Button 1 controls SCO audio on/off */
void bt_hfp_hf_ptt_press(void)
{
    if (!bt_hfp_is_connected()) {
        ESP_LOGW(TAG, "PTT: HFP not connected, ignoring");
        return;
    }
    if (bt_audio_is_active()) {
        ESP_LOGI(TAG, "PTT: audio already active (AG-opened)");
        return;
    }
    ESP_LOGI(TAG, "PTT press — opening audio to AG");
    s_ptt_opened_audio = true;
    esp_hf_client_connect_audio(hf_peer_addr);
}

void bt_hfp_hf_ptt_release(void)
{
    if (!bt_audio_is_active()) return;
    ESP_LOGI(TAG, "PTT release — stopping pipeline (SCO teardown left to AG)");
    bt_hfp_hf_audio_pipeline_stop();
    s_ptt_opened_audio = false;
}

/* HFP HF Client event strings */
static const char *c_hf_evt_str[] = {
    "CONNECTION_STATE_EVT",
    "AUDIO_STATE_EVT",
    "BVRA_EVT",
    "CIND_CALL_EVT",
    "CIND_CALL_SETUP_EVT",
    "CIND_CALL_HELD_EVT",
    "CIND_SERVICE_AVAILABILITY_EVT",
    "CIND_SIGNAL_STRENGTH_EVT",
    "CIND_ROAMING_STATUS_EVT",
    "CIND_BATTERY_LEVEL_EVT",
    "COPS_CURRENT_OPERATOR_EVT",
    "BTRH_EVT",
    "CLIP_EVT",
    "CCWA_EVT",
    "CLCC_EVT",
    "VOLUME_CONTROL_EVT",
    "AT_RESPONSE_EVT",
    "CNUM_EVT",
    "BSIR_EVT",
    "BINP_EVT",
    "RING_IND_EVT",
    "PKT_STAT_NUMS_GET_EVT",
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
    "DISCONNECTED",
    "CONNECTING",
    "CONNECTED",
    "CONNECTED_MSBC",
};

void bt_app_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param)
{
    if (event <= ESP_HF_CLIENT_PROF_STATE_EVT) {
        ESP_LOGI(TAG, "HFP HF event: %s", c_hf_evt_str[event]);
    } else {
        ESP_LOGE(TAG, "HFP HF invalid event %d", event);
    }

    switch (event) {
    case ESP_HF_CLIENT_CONNECTION_STATE_EVT: {
        ESP_LOGI(TAG, "connection state %s, peer feats 0x%" PRIx32 ", chld feats 0x%" PRIx32,
                 c_connection_state_str[param->conn_stat.state],
                 param->conn_stat.peer_feat,
                 param->conn_stat.chld_feat);

        memcpy(hf_peer_addr, param->conn_stat.remote_bda, ESP_BD_ADDR_LEN);

        bool connected = (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED);
        bt_hfp_set_connected(connected);

        extern void ble_send_device_status(uint8_t hfp_connected, uint8_t audio_active);
        ble_send_device_status(bt_hfp_is_connected(), bt_audio_is_active());

        if (connected) {
            ESP_LOGI(TAG, "SLC connected, ready for PTT");
        }
        break;
    }

    case ESP_HF_CLIENT_AUDIO_STATE_EVT: {
        ESP_LOGI(TAG, "Audio State: %s", c_audio_state_str[param->audio_stat.state]);

#if CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI
        if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED ||
            param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) {

            s_audio_codec = param->audio_stat.state;

            /* Register legacy data callbacks — safe in BTU_TASK */
            esp_hf_client_register_data_callback(bt_app_hf_incoming_cb, bt_app_hf_outgoing_cb);

            bt_audio_set_active(true);

            /* Defer heavy work (ringbuf, timer, task, I2S) to app task */
            bt_app_work_dispatch(audio_start_handler, 0, NULL, 0, NULL);

            extern void ble_send_device_status(uint8_t hfp_connected, uint8_t audio_active);
            ble_send_device_status(bt_hfp_is_connected(), bt_audio_is_active());

        } else if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED) {
            ESP_LOGI(TAG, "Audio disconnected");
            s_ptt_opened_audio = false;
            bt_audio_set_active(false);

            /* Defer heavy cleanup to app task */
            bt_app_work_dispatch(audio_stop_handler, 0, NULL, 0, NULL);

            extern void ble_send_device_status(uint8_t hfp_connected, uint8_t audio_active);
            ble_send_device_status(bt_hfp_is_connected(), bt_audio_is_active());
        }
#endif
        break;
    }

    case ESP_HF_CLIENT_BVRA_EVT:
        ESP_LOGI(TAG, "Voice Recognition: %d", param->bvra.value);
        break;

    case ESP_HF_CLIENT_VOLUME_CONTROL_EVT:
        ESP_LOGI(TAG, "Volume: type=%d, vol=%d",
                 param->volume_control.type, param->volume_control.volume);
        break;

    case ESP_HF_CLIENT_CIND_SERVICE_AVAILABILITY_EVT:
        ESP_LOGI(TAG, "Service availability: %d", param->service_availability.status);
        break;

    case ESP_HF_CLIENT_CIND_SIGNAL_STRENGTH_EVT:
        ESP_LOGI(TAG, "Signal strength: %d", param->signal_strength.value);
        break;

    case ESP_HF_CLIENT_CIND_BATTERY_LEVEL_EVT:
        ESP_LOGI(TAG, "Battery level: %d", param->battery_level.value);
        break;

    case ESP_HF_CLIENT_CIND_ROAMING_STATUS_EVT:
        ESP_LOGI(TAG, "Roaming: %d", param->roaming.status);
        break;

    case ESP_HF_CLIENT_CIND_CALL_EVT:
        ESP_LOGI(TAG, "Call status: %d", param->call.status);
        break;

    case ESP_HF_CLIENT_CIND_CALL_SETUP_EVT:
        ESP_LOGI(TAG, "Call setup: %d", param->call_setup.status);
        break;

    case ESP_HF_CLIENT_CIND_CALL_HELD_EVT:
        ESP_LOGI(TAG, "Call held: %d", param->call_held.status);
        break;

    case ESP_HF_CLIENT_COPS_CURRENT_OPERATOR_EVT:
        ESP_LOGI(TAG, "Operator: %s", param->cops.name);
        break;

    case ESP_HF_CLIENT_CLIP_EVT:
        ESP_LOGI(TAG, "CLIP number: %s", param->clip.number);
        break;

    case ESP_HF_CLIENT_CLCC_EVT:
        ESP_LOGI(TAG, "Current call: idx=%d dir=%d status=%d number=%s",
                 param->clcc.idx, param->clcc.dir, param->clcc.status, param->clcc.number);
        break;

    case ESP_HF_CLIENT_RING_IND_EVT:
        ESP_LOGI(TAG, "Ring indication");
        break;

    case ESP_HF_CLIENT_AT_RESPONSE_EVT:
        ESP_LOGI(TAG, "AT response: code=%d, cme=%d",
                 param->at_response.code, param->at_response.cme);
        break;

    case ESP_HF_CLIENT_CNUM_EVT:
        ESP_LOGI(TAG, "Subscriber number: %s, type=%d",
                 param->cnum.number, param->cnum.type);
        break;

    case ESP_HF_CLIENT_BSIR_EVT:
        ESP_LOGI(TAG, "In-band ring: %d", param->bsir.state);
        break;

    case ESP_HF_CLIENT_BINP_EVT:
        ESP_LOGI(TAG, "Voice tag: %s", param->binp.number);
        break;

    case ESP_HF_CLIENT_PKT_STAT_NUMS_GET_EVT:
        ESP_LOGI(TAG, "Packet stats event");
        break;

    case ESP_HF_CLIENT_PROF_STATE_EVT:
        if (param->prof_stat.state == ESP_HF_INIT_SUCCESS) {
            ESP_LOGI(TAG, "HF PROF STATE: Init Complete");
        } else if (param->prof_stat.state == ESP_HF_DEINIT_SUCCESS) {
            ESP_LOGI(TAG, "HF PROF STATE: Deinit Complete");
        } else {
            ESP_LOGE(TAG, "HF PROF STATE error: %d", param->prof_stat.state);
        }
        break;

    default:
        ESP_LOGI(TAG, "Unhandled HFP HF event: %d", event);
        break;
    }
}
