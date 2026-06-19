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

/*
 * on_sync: called by NimBLE host when the host<->controller link is ready.
 * This is the CORRECT place to call ble_store_config_init() — at this point
 * the NimBLE NVS store backend is fully initialised and can read/write bonds.
 * Calling it before (e.g. in app_main before esp_nimble_enable) causes the
 * LTK to not be properly restored from NVS, leading to status=7 on Windows
 * reconnect.
 */
static void nimble_on_sync(void)
{
    ESP_LOGI(TAG, "NimBLE host synced — initialising bond store");

    /* Initialise the NVS-backed bond store NOW (host is ready). */
    ble_store_config_init();

    /* Restore address — use public address from efuse */
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed: %d", rc);
    }

    ESP_LOGI(TAG, "NimBLE store ready, starting advertising");
    esp_hid_ble_gap_adv_start();
}

/* HID device event callback */
static void hid_event_cb(void *handler_args, esp_event_base_t base,
                         int32_t id, void *event_data)
{
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (id) {
    case ESP_HIDD_START_EVENT:
        /* Advertising is already started by nimble_on_sync(); do NOT call
         * esp_hid_ble_gap_adv_start() here or NimBLE will report
         * "advertising already started" and may reset the adv state. */
        ESP_LOGI(TAG, "HID device started (advertising managed by on_sync)");
        break;
    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "HID connected");
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "HID disconnected — restarting advertising");
        /* Restart advertising so Windows can reconnect */
        esp_hid_ble_gap_adv_start();
        break;
    case ESP_HIDD_OUTPUT_EVENT:
        break;
    default:
        break;
    }
}

static int my_store_read_cb(int obj_type, const union ble_store_key *key, union ble_store_value *val)
{
    ESP_LOGI(TAG, "NVS Read: obj_type=%d", obj_type);
    if (obj_type == BLE_STORE_OBJ_TYPE_OUR_SEC || obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
        ESP_LOGI(TAG, "  Key Query: peer_addr=%02X:%02X:%02X:%02X:%02X:%02X",
                 key->sec.peer_addr.val[5], key->sec.peer_addr.val[4], key->sec.peer_addr.val[3],
                 key->sec.peer_addr.val[2], key->sec.peer_addr.val[1], key->sec.peer_addr.val[0]);
    }
    extern int ble_store_config_read(int obj_type, const union ble_store_key *key, union ble_store_value *val);
    int rc = ble_store_config_read(obj_type, key, val);
    ESP_LOGI(TAG, "NVS Read Result: rc=%d", rc);
    if (rc == 0 && (obj_type == BLE_STORE_OBJ_TYPE_OUR_SEC || obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC)) {
        ESP_LOGI(TAG, "  Found Sec: ediv=%u, rand=%" PRIu64 ", sc=%d, key_size=%d, LTK[0..3]=%02X%02X%02X%02X",
                 val->sec.ediv, val->sec.rand_num, val->sec.sc, val->sec.key_size,
                 val->sec.ltk[0], val->sec.ltk[1], val->sec.ltk[2], val->sec.ltk[3]);
    }
    return rc;
}

static int my_store_write_cb(int obj_type, const union ble_store_value *val)
{
    ESP_LOGI(TAG, "NVS Write: obj_type=%d", obj_type);
    if (obj_type == BLE_STORE_OBJ_TYPE_OUR_SEC || obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
        ESP_LOGI(TAG, "  Writing Sec: peer_addr=%02X:%02X:%02X:%02X:%02X:%02X, ediv=%u, rand=%" PRIu64 ", sc=%d, key_size=%d, LTK[0..3]=%02X%02X%02X%02X",
                 val->sec.peer_addr.val[5], val->sec.peer_addr.val[4], val->sec.peer_addr.val[3],
                 val->sec.peer_addr.val[2], val->sec.peer_addr.val[1], val->sec.peer_addr.val[0],
                 val->sec.ediv, val->sec.rand_num, val->sec.sc, val->sec.key_size,
                 val->sec.ltk[0], val->sec.ltk[1], val->sec.ltk[2], val->sec.ltk[3]);
    }
    extern int ble_store_config_write(int obj_type, const union ble_store_value *val);
    int rc = ble_store_config_write(obj_type, val);
    ESP_LOGI(TAG, "NVS Write Result: rc=%d", rc);
    return rc;
}


static int my_store_delete_cb(int obj_type, const union ble_store_key *key)
{
    ESP_LOGI(TAG, "NVS Delete: obj_type=%d", obj_type);
    extern int ble_store_config_delete(int obj_type, const union ble_store_key *key);
    int rc = ble_store_config_delete(obj_type, key);
    ESP_LOGI(TAG, "NVS Delete Result: rc=%d", rc);
    return rc;
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
    ESP_LOGI(TAG, "  ESP32 BLE HID Keyboard — NimBLE (v7-force-store)");
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
    /* NOTE: adv_init sets security params in ble_hs_cfg but does NOT start advertising.
     * Advertising is now started inside nimble_on_sync() after host is ready. */
    esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, dev_name);

    /*
     * Register NimBLE store callbacks & sync callback AFTER NimBLE initialization
     * to prevent them from being overwritten during esp_nimble_init().
     */
    ble_hs_cfg.store_read_cb   = my_store_read_cb;
    ble_hs_cfg.store_write_cb  = my_store_write_cb;
    ble_hs_cfg.store_delete_cb = my_store_delete_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sync_cb         = nimble_on_sync;

    ESP_LOGI(TAG, "Step 5: Initializing HID Keyboard...");
    ble_hid_keyboard_init();

    ESP_LOGI(TAG, "Step 6: Enabling NimBLE host...");
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
