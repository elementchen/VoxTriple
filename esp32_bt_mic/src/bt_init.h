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
 * @brief Gracefully disconnect HFP service-level connection from current peer
 */
esp_err_t bt_hfp_disconnect(void);

#endif /* __BT_INIT_H__ */
