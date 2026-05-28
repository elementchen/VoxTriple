/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * BLE HID Keyboard device — sends HID keyboard reports directly
 * to Windows/Mac without requiring a companion app.
 */

#ifndef __BLE_HID_KEYBOARD_H__
#define __BLE_HID_KEYBOARD_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize BLE HID Keyboard device.
 *        After this call, the device advertises as "ESP32_BT_KeyBoard"
 *        and Windows/Mac will auto-connect to it as a keyboard.
 * @return ESP_OK on success
 */
esp_err_t ble_hid_keyboard_init(void);

/**
 * @brief Check if HID keyboard is currently connected to a host.
 * @return true if connected
 */
bool ble_hid_is_connected(void);

/**
 * @brief Send a HID keyboard report (key press or release).
 *
 * @param vk_code   Windows virtual key code (from button mapping)
 * @param modifier  Modifier bitmask (same format as button mapping)
 * @param pressed   true = key down, false = key up (all-zero report)
 */
void ble_hid_send_key(uint8_t vk_code, uint8_t modifier, bool pressed);

#endif /* __BLE_HID_KEYBOARD_H__ */
