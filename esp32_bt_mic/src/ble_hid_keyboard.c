/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * BLE HID Keyboard — sends HID reports directly to Windows/Mac.
 * Runs alongside the custom GATT service (0x1820) on the same BLE connection.
 */

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_hidd.h"
#include "esp_hid_common.h"
#include "ble_hid_keyboard.h"

static const char *TAG = "BLE_HID_KBD";

/* ----------------------------------------------------------------
 * HID Report Map — standard boot keyboard (8-byte report)
 * ---------------------------------------------------------------- */
static const uint8_t s_hid_report_map[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,        //   Usage Minimum (224)
    0x29, 0xE7,        //   Usage Maximum (231)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data,Var,Abs) — modifier byte
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x03,        //   Input (Const) — reserved byte
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum
    0x29, 0x05,        //   Usage Maximum
    0x91, 0x02,        //   Output (Data,Var,Abs) — LED status
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x03,        //   Output (Const)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data,Array) — key array (5 bytes)
    0xC0,              // End Collection
};

static esp_hidd_dev_t *s_hid_dev = NULL;
static bool s_hid_connected = false;
extern char g_bt_device_name[];

/* ----------------------------------------------------------------
 * Windows VK code → USB HID Usage ID mapping
 * Used for alphanumeric, punctuation, and common special keys.
 * Modifier keys (Ctrl/Shift/Alt/Win) are handled via modifier byte.
 * ---------------------------------------------------------------- */
static const uint8_t s_vk_to_hid[256] = {
    [0x08] = 0x2A,  // VK_BACK     → Backspace
    [0x09] = 0x2B,  // VK_TAB      → Tab
    [0x0D] = 0x28,  // VK_RETURN   → Enter
    [0x10] = 0x00,  // VK_SHIFT    → modifier only
    [0x11] = 0x00,  // VK_CONTROL  → modifier only
    [0x12] = 0x00,  // VK_MENU     → modifier only (Alt)
    [0x13] = 0x48,  // VK_PAUSE    → Pause
    [0x14] = 0x39,  // VK_CAPITAL  → Caps Lock
    [0x1B] = 0x29,  // VK_ESCAPE   → Escape
    [0x20] = 0x2C,  // VK_SPACE    → Spacebar
    [0x21] = 0x4B,  // VK_PRIOR    → Page Up
    [0x22] = 0x4E,  // VK_NEXT     → Page Down
    [0x23] = 0x4D,  // VK_END      → End
    [0x24] = 0x4A,  // VK_HOME     → Home
    [0x25] = 0x50,  // VK_LEFT     → Left Arrow
    [0x26] = 0x52,  // VK_UP       → Up Arrow
    [0x27] = 0x4F,  // VK_RIGHT    → Right Arrow
    [0x28] = 0x51,  // VK_DOWN     → Down Arrow
    [0x2C] = 0x46,  // VK_SNAPSHOT → Print Screen
    [0x2D] = 0x49,  // VK_INSERT   → Insert
    [0x2E] = 0x4C,  // VK_DELETE   → Delete Forward
    [0x30] = 0x27,  // 0
    [0x31] = 0x1E,  // 1
    [0x32] = 0x1F,  // 2
    [0x33] = 0x20,  // 3
    [0x34] = 0x21,  // 4
    [0x35] = 0x22,  // 5
    [0x36] = 0x23,  // 6
    [0x37] = 0x24,  // 7
    [0x38] = 0x25,  // 8
    [0x39] = 0x26,  // 9
    [0x41] = 0x04,  // A
    [0x42] = 0x05,  // B
    [0x43] = 0x06,  // C
    [0x44] = 0x07,  // D
    [0x45] = 0x08,  // E
    [0x46] = 0x09,  // F
    [0x47] = 0x0A,  // G
    [0x48] = 0x0B,  // H
    [0x49] = 0x0C,  // I
    [0x4A] = 0x0D,  // J
    [0x4B] = 0x0E,  // K
    [0x4C] = 0x0F,  // L
    [0x4D] = 0x10,  // M
    [0x4E] = 0x11,  // N
    [0x4F] = 0x12,  // O
    [0x50] = 0x13,  // P
    [0x51] = 0x14,  // Q
    [0x52] = 0x15,  // R
    [0x53] = 0x16,  // S
    [0x54] = 0x17,  // T
    [0x55] = 0x18,  // U
    [0x56] = 0x19,  // V
    [0x57] = 0x1A,  // W
    [0x58] = 0x1B,  // X
    [0x59] = 0x1C,  // Y
    [0x5A] = 0x1D,  // Z
    [0x5B] = 0x00,  // VK_LWIN    → modifier only
    [0x5C] = 0x00,  // VK_RWIN    → modifier only
    [0x5D] = 0x65,  // VK_APPS    → Application (menu key)
    [0x60] = 0x62,  // VK_NUMPAD0 → Keypad 0 / Insert
    [0x61] = 0x59,  // VK_NUMPAD1 → Keypad 1 / End
    [0x62] = 0x5A,  // VK_NUMPAD2 → Keypad 2 / Down
    [0x63] = 0x5B,  // VK_NUMPAD3 → Keypad 3 / PgDn
    [0x64] = 0x5C,  // VK_NUMPAD4 → Keypad 4 / Left
    [0x65] = 0x5D,  // VK_NUMPAD5 → Keypad 5
    [0x66] = 0x5E,  // VK_NUMPAD6 → Keypad 6 / Right
    [0x67] = 0x5F,  // VK_NUMPAD7 → Keypad 7 / Home
    [0x68] = 0x60,  // VK_NUMPAD8 → Keypad 8 / Up
    [0x69] = 0x61,  // VK_NUMPAD9 → Keypad 9 / PgUp
    [0x6A] = 0x55,  // VK_MULTIPLY → Keypad *
    [0x6B] = 0x57,  // VK_ADD     → Keypad +
    [0x6D] = 0x56,  // VK_SUBTRACT→ Keypad -
    [0x6E] = 0x63,  // VK_DECIMAL → Keypad .
    [0x6F] = 0x54,  // VK_DIVIDE  → Keypad /
    [0x70] = 0x3A,  // F1
    [0x71] = 0x3B,  // F2
    [0x72] = 0x3C,  // F3
    [0x73] = 0x3D,  // F4
    [0x74] = 0x3E,  // F5
    [0x75] = 0x3F,  // F6
    [0x76] = 0x40,  // F7
    [0x77] = 0x41,  // F8
    [0x78] = 0x42,  // F9
    [0x79] = 0x43,  // F10
    [0x7A] = 0x44,  // F11
    [0x7B] = 0x45,  // F12
    [0x90] = 0x53,  // VK_NUMLOCK → Num Lock / Clear
    [0xA0] = 0x00,  // VK_LSHIFT  → modifier only
    [0xA1] = 0x00,  // VK_RSHIFT  → modifier only
    [0xA2] = 0x00,  // VK_LCONTROL→ modifier only
    [0xA3] = 0x00,  // VK_RCONTROL→ modifier only
    [0xA4] = 0x00,  // VK_LMENU   → modifier only (Alt)
    [0xA5] = 0x00,  // VK_RMENU   → modifier only (Alt)
    [0xAD] = 0xE2,  // VK_VOLUME_MUTE
    [0xAE] = 0xEA,  // VK_VOLUME_DOWN
    [0xAF] = 0xE9,  // VK_VOLUME_UP
    [0xB0] = 0xB6,  // VK_MEDIA_NEXT_TRACK
    [0xB1] = 0xB5,  // VK_MEDIA_PREV_TRACK
    [0xB3] = 0xCD,  // VK_MEDIA_PLAY_PAUSE
    [0xBA] = 0x33,  // VK_OEM_1    → ; and :
    [0xBB] = 0x2E,  // VK_OEM_PLUS → = and +
    [0xBC] = 0x36,  // VK_OEM_COMMA→ , and <
    [0xBD] = 0x2D,  // VK_OEM_MINUS→ - and _
    [0xBE] = 0x37,  // VK_OEM_PERIOD→ . and >
    [0xBF] = 0x38,  // VK_OEM_2    → / and ?
    [0xC0] = 0x35,  // VK_OEM_3    → ` and ~
    [0xDB] = 0x2F,  // VK_OEM_4    → [ and {
    [0xDC] = 0x31,  // VK_OEM_5    → \ and |
    [0xDD] = 0x30,  // VK_OEM_6    → ] and }
    [0xDE] = 0x34,  // VK_OEM_7    → ' and "
};

/* Modifier bitmask helpers */
static inline uint8_t vk_mod_to_hid(uint8_t mod_mask)
{
    uint8_t hid_mod = 0;
    if (mod_mask & 0x01) hid_mod |= 0x01;  // LCtrl
    if (mod_mask & 0x02) hid_mod |= 0x02;  // LShift
    if (mod_mask & 0x04) hid_mod |= 0x04;  // LAlt
    if (mod_mask & 0x08) hid_mod |= 0x08;  // LGUI (Win/Cmd)
    if (mod_mask & 0x10) hid_mod |= 0x10;  // RCtrl
    if (mod_mask & 0x20) hid_mod |= 0x20;  // RShift
    if (mod_mask & 0x40) hid_mod |= 0x40;  // RAlt
    if (mod_mask & 0x80) hid_mod |= 0x80;  // RGUI (Win/Cmd)
    return hid_mod;
}

/* ----------------------------------------------------------------
 * HID event callback — handles connection state and re-advertising
 * ---------------------------------------------------------------- */
static void hid_event_cb(void *handler_args, esp_event_base_t base,
                         int32_t id, void *event_data)
{
    switch ((esp_hidd_event_t)id) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "HID device started");
        break;
    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "HID connected");
        s_hid_connected = true;
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "HID disconnected");
        s_hid_connected = false;
        break;
    default:
        break;
    }
}

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */

esp_err_t ble_hid_keyboard_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE HID Keyboard");

    esp_hid_device_config_t hid_config = {
        .vendor_id          = 0x16C0,
        .product_id         = 0x05DF,
        .version            = 0x0100,
        .device_name        = "ESP32_BT_KeyBoard",
        .manufacturer_name  = "VoxTriple",
        .serial_number      = "0001",
    };

    esp_hid_raw_report_map_t report_maps[1] = {{
        .data = s_hid_report_map,
        .len  = sizeof(s_hid_report_map),
    }};

    hid_config.report_maps     = report_maps;
    hid_config.report_maps_len = 1;

    /* HID registers its own GATT callback; our unified handler in
     * ble_gatts_config.c will dispatch to esp_hidd_gatts_event_handler. */
    esp_err_t ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler);
    if (ret) {
        ESP_LOGE(TAG, "HID GATTS register failed: %d", ret);
        return ret;
    }

    ret = esp_hidd_dev_init(&hid_config, ESP_HID_TRANSPORT_BLE,
                            hid_event_cb, &s_hid_dev);
    if (ret) {
        ESP_LOGE(TAG, "HID dev init failed: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "BLE HID Keyboard initialized");
    return ESP_OK;
}

bool ble_hid_is_connected(void)
{
    return s_hid_connected;
}

void ble_hid_send_key(uint8_t vk_code, uint8_t modifier, bool pressed)
{
    if (!s_hid_dev) return;

    /* Standard boot keyboard input report (7 bytes without report ID):
     * [0]=modifier  [1]=reserved(0)  [2-6]=keycode slots */
    uint8_t report[7] = {0};

    if (pressed) {
        report[0] = vk_mod_to_hid(modifier);
        report[2] = s_vk_to_hid[vk_code];
    }
    /* On release: send all-zero report (key up) */

    esp_hidd_dev_input_set(s_hid_dev, 0, 1, report, sizeof(report));
}
