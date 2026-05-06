/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BLE_GATTS_CONFIG_H__
#define __BLE_GATTS_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

/* Button mapping: 2 bytes [vk_code, modifier bitmask] */
#define BTN_MAP_LEN  2

/* Modifier key bitmask definitions (Windows Virtual Key modifiers) */
#define MOD_LCTRL    (1 << 0)
#define MOD_LSHIFT   (1 << 1)
#define MOD_LALT     (1 << 2)
#define MOD_LWIN     (1 << 3)
#define MOD_RCTRL    (1 << 4)
#define MOD_RSHIFT   (1 << 5)
#define MOD_RALT     (1 << 6)
#define MOD_RWIN     (1 << 7)

/* Windows Virtual Key codes */
#define VK_LCONTROL  0xA2
#define VK_LSHIFT    0xA0
#define VK_LMENU     0xA4
#define VK_LWIN      0x5B
#define VK_RCONTROL  0xA3
#define VK_RSHIFT    0xA1
#define VK_RMENU     0xA5
#define VK_RWIN      0x5C

/* Button IDs */
#define BUTTON_ID_1   0
#define BUTTON_ID_2   1
#define BUTTON_ID_3   2

/* Device status */
#define DEVICE_STATUS_LEN  2

/**
 * @brief Initialize BLE GATT server with button mapping service
 */
void ble_gatts_init(void);

/**
 * @brief Send button event notification via BLE
 * @param button_id  Button index (0-2)
 * @param state      0=released, 1=pressed
 */
void ble_send_button_event(uint8_t button_id, uint8_t state);

/**
 * @brief Send device status notification via BLE
 * @param hfp_connected  1 if HFP connected, 0 otherwise
 * @param audio_active   1 if audio is streaming, 0 otherwise
 */
void ble_send_device_status(uint8_t hfp_connected, uint8_t audio_active);

/**
 * @brief Get button mapping for a given button
 * @param button_id  Button index (0-2)
 * @param vk_code    Output: virtual key code
 * @param modifier   Output: modifier bitmask
 */
void ble_get_button_mapping(uint8_t button_id, uint8_t *vk_code, uint8_t *modifier);

/**
 * @brief Stop BLE advertising (call when SCO audio is active to avoid interference)
 */
void ble_gatts_adv_stop(void);

/**
 * @brief Restart BLE advertising (call when SCO audio stops)
 */
void ble_gatts_adv_start(void);

#endif /* __BLE_GATTS_CONFIG_H__ */
