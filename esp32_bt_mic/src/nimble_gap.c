/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * NimBLE GAP + custom GATT service for BLE-only HID keyboard.
 * Replaces ble_gatts_config.c when using NimBLE stack.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "ble_gatts_config.h"
#include "config_storage.h"
#include "bt_init.h"

static const char *TAG = "NIMBLE_GAP";

/* ── Connection state ─────────────────────────────────────────── */
static bool s_ble_connected = false;
static uint16_t s_conn_handle = 0;

/* ── Advertising ──────────────────────────────────────────────── */
static TimerHandle_t s_adv_timer = NULL;

static void adv_restart_cb(TimerHandle_t xTimer)
{
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, 180000,
                      NULL, NULL, NULL);
    ESP_LOGI(TAG, "BLE advertising restarted");
}

/* ── GAP event callback ───────────────────────────────────────── */
static int nimble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_ble_connected = true;
            ESP_LOGI(TAG, "BLE connected, handle %d", s_conn_handle);
            /* Start security — NimBLE handles bond automatically */
            ble_gap_security_initiate(s_conn_handle);
        } else {
            ESP_LOGE(TAG, "Connection failed: %d", event->connect.status);
            /* Restart advertising after 3s */
            if (s_adv_timer) xTimerStart(s_adv_timer, 0);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnected, reason %d",
                 event->disconnect.reason);
        s_ble_connected = false;
        s_conn_handle = 0;
        /* Restart advertising after 3s */
        if (s_adv_timer) xTimerStart(s_adv_timer, 0);
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "Encryption established");
        } else {
            ESP_LOGW(TAG, "Encryption failed: %d",
                     event->enc_change.status);
        }
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        ESP_LOGI(TAG, "Repeat pairing — starting new pairing");
        ble_gap_security_initiate(event->repeat_pairing.conn_handle);
        break;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(TAG, "Passkey action: %d", event->passkey.params.action);
        if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
            ble_sm_inject_io(event->passkey.conn_handle,
                             &(struct ble_sm_io){ .passkey = 0, .action = BLE_SM_IOACT_NONE });
        } else if (event->passkey.params.action == BLE_SM_IOACT_NONE) {
            ble_sm_inject_io(event->passkey.conn_handle, NULL);
        }
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update: %d", event->mtu.value);
        break;

    case BLE_GAP_EVENT_NOTIFY_TX:
        break;

    default:
        break;
    }
    return 0;
}

/* ── Start advertising ─────────────────────────────────────────── */
static void start_advertising(void)
{
    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };

    /* Advertisement data — include device name, appearance, HID UUID */
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN;
    fields.appearance = 0x03C1;  /* HID Keyboard */
    fields.appearance_is_present = 1;
    fields.tx_pwr_lvl_is_present = 1;
    fields.name = (uint8_t *)g_bt_device_name;
    fields.name_len = strlen(g_bt_device_name);
    fields.name_is_complete = 1;

    /* Include HID service UUID (0x1812) in advertising */
    uint8_t hid_uuid[2] = {0x12, 0x18};  /* 0x1812 LE byte order */
    fields.uuids16 = (ble_uuid16_t *)hid_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, 180000,
                      &adv_params, nimble_gap_event, NULL);
    ESP_LOGI(TAG, "BLE advertising started as '%s'", g_bt_device_name);
}

/* ── Public API ────────────────────────────────────────────────── */
void app_ble_gap_init(void)
{
    ESP_LOGI(TAG, "Initializing NimBLE GAP");

    /* Set appearance to HID Keyboard */
    ble_svc_gap_device_appearance_set(0x03C1);

    /* Configure security: SC bonding, Just Works */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    /* Create retry timer */
    s_adv_timer = xTimerCreate("adv_retry", pdMS_TO_TICKS(3000),
                                pdFALSE, NULL, adv_restart_cb);

    /* Start advertising */
    start_advertising();

    ESP_LOGI(TAG, "NimBLE GAP initialized");
}

void ble_gatts_adv_stop(void)
{
    ble_gap_adv_stop();
    ESP_LOGI(TAG, "BLE advertising stopped");
}

void ble_gatts_adv_start(void)
{
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, 180000,
                      NULL, nimble_gap_event, NULL);
    ESP_LOGI(TAG, "BLE advertising restarted");
}

bool ble_gatts_is_connected(void)
{
    return s_ble_connected;
}

/* ── Button event notification (stub — custom service not yet ported) ── */
void ble_send_button_event(uint8_t button_id, uint8_t state)
{
    (void)button_id; (void)state;
    /* TODO: port custom 0x1820 service to NimBLE */
}

void ble_send_device_status(uint8_t hfp, uint8_t audio)
{
    (void)hfp; (void)audio;
    /* TODO: port custom 0x1820 service to NimBLE */
}

void ble_get_button_mapping(uint8_t btn, uint8_t *vk, uint8_t *mod)
{
    /* Return defaults — config storage read would be re-added */
    static const uint8_t defaults_vk[4]  = {0x0D, 0x1B, 0x20, 0x09};
    static const uint8_t defaults_mod[4] = {0x00, 0x00, 0x00, 0x00};
    if (btn < 4) { *vk = defaults_vk[btn]; *mod = defaults_mod[btn]; }
}

uint8_t ble_get_button_vk(uint8_t btn)   { uint8_t v=0,m=0; ble_get_button_mapping(btn,&v,&m); return v; }
uint8_t ble_get_button_mod(uint8_t btn)  { uint8_t v=0,m=0; ble_get_button_mapping(btn,&v,&m); return m; }

