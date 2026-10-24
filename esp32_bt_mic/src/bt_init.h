/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BT_INIT_H__
#define __BT_INIT_H__

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Current Bluetooth device broadcast name
 */
extern char g_bt_device_name[32];

/**
 * @brief Initialize NVS flash storage
 * @return ESP_OK on success
 */
esp_err_t bt_nvs_init(void);

/**
 * @brief Initialize Bluetooth controller and Bluedroid stack (dual mode)
 * @return ESP_OK on success
 */
esp_err_t bt_stack_init(void);

/**
 * @brief Get current HFP connection state
 * @return true if HFP AG is connected to a peer
 */
bool bt_hfp_is_connected(void);

/**
 * @brief Get current audio connection state
 * @return true if audio SCO channel is active
 */
bool bt_audio_is_active(void);

/**
 * @brief Set HFP connection state (called from HFP callback)
 */
void bt_hfp_set_connected(bool connected);

/**
 * @brief Set audio active state (called from HFP callback)
 */
void bt_audio_set_active(bool active);

/**
 * @brief Force HFP ACL out of sniff mode to reduce SCO open latency.
 *        Should be called on button press (before Windows opens SCO).
 */
void bt_hfp_hf_wake_acl(void);

/**
 * @brief Activate Classic BT HFP SCO channel (PTT active)
 */
void bt_classic_activate(void);

/**
 * @brief Deactivate Classic BT HFP SCO channel (PTT released)
 */
void bt_classic_deactivate(void);

/**
 * @brief Get whether PTT mode is currently activated
 */
bool bt_classic_is_ptt_activated(void);

#endif /* __BT_INIT_H__ */
