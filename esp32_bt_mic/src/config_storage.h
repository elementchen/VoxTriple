/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __CONFIG_STORAGE_H__
#define __CONFIG_STORAGE_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_bt_defs.h"
#include "esp_err.h"

#define NVS_NAMESPACE  "bt_mic_cfg"
#define BUTTON_NUM     3

/* NVS key names for button mappings */
#define NVS_KEY_BTN1_MAP  "btn1_map"
#define NVS_KEY_BTN2_MAP  "btn2_map"
#define NVS_KEY_BTN3_MAP  "btn3_map"

/**
 * @brief Initialize NVS configuration storage
 *        Loads saved button mappings or sets defaults
 */
void config_storage_init(void);

/**
 * @brief Save button mapping to NVS
 * @param button_id  Button index (0-2)
 * @param vk_code    Windows virtual key code
 * @param modifier   Modifier bitmask
 * @return ESP_OK on success
 */
esp_err_t config_storage_save_button(uint8_t button_id, uint8_t vk_code, uint8_t modifier);

/**
 * @brief Load button mapping from NVS
 * @param button_id  Button index (0-2)
 * @param vk_code    Output: virtual key code
 * @param modifier   Output: modifier bitmask
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if not saved
 */
esp_err_t config_storage_load_button(uint8_t button_id, uint8_t *vk_code, uint8_t *modifier);

/**
 * @brief Save paired HFP device address for BLE-triggered auto-reconnect
 */
esp_err_t config_storage_save_hfp_addr(esp_bd_addr_t addr);

/**
 * @brief Load last paired HFP device address
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no address saved
 */
esp_err_t config_storage_load_hfp_addr(esp_bd_addr_t addr);

#endif /* __CONFIG_STORAGE_H__ */
