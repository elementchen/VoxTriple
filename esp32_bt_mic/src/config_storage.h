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
#define BUTTON_NUM     4

/* NVS key names for button mappings */
#define NVS_KEY_BTN1_MAP  "btn1_map"
#define NVS_KEY_BTN2_MAP  "btn2_map"
#define NVS_KEY_BTN3_MAP  "btn3_map"
#define NVS_KEY_BTN4_MAP  "btn4_map"
#define NVS_KEY_SLEEP_MODE "sleep_mode"
#define NVS_KEY_MIC_ENABLED "mic_enabled"
#define NVS_KEY_TX_POWER   "tx_power"
#define NVS_KEY_OTA_READY      "ota_ready"
#define NVS_KEY_BOARD_MODEL    "hw_model"
#define NVS_KEY_SLEEP_TIMEOUT  "sleep_tmout"

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

/**
 * @brief Clear paired HFP device address from NVS
 * @return ESP_OK on success
 */
esp_err_t config_storage_clear_hfp_addr(void);

/**
 * @brief Multi-host slot functions
 */
esp_err_t config_storage_get_active_slot(uint8_t *slot);
esp_err_t config_storage_set_active_slot(uint8_t slot);
esp_err_t config_storage_load_slot_hfp_addr(uint8_t slot, esp_bd_addr_t addr);
esp_err_t config_storage_save_slot_hfp_addr(uint8_t slot, const esp_bd_addr_t addr);
esp_err_t config_storage_clear_slot_hfp_addr(uint8_t slot);

/**
 * @brief Save Classic BT TX power level to NVS
 * @param level  Power level (0-7, maps to ESP_PWR_LVL_N12..ESP_PWR_LVL_P9)
 * @return ESP_OK on success
 */
esp_err_t config_storage_save_tx_power(uint8_t level);

/**
 * @brief Load Classic BT TX power level from NVS
 * @param level  Output: power level (0-7)
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if not saved
 */
esp_err_t config_storage_load_tx_power(uint8_t *level);

/**
 * @brief Save BT controller sleep mode to NVS
 * @param enabled  0=disabled, 1=enabled
 * @return ESP_OK on success
 */
esp_err_t config_storage_save_sleep_mode(uint8_t enabled);

/**
 * @brief Load BT controller sleep mode from NVS
 * @param enabled  Output: 0=disabled, 1=enabled
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if not saved
 */
esp_err_t config_storage_load_sleep_mode(uint8_t *enabled);

/**
 * @brief Save HFP microphone enablement mode to NVS
 * @param enabled  0=disabled, 1=enabled
 * @return ESP_OK on success
 */
esp_err_t config_storage_save_mic_enabled(uint8_t enabled);

/**
 * @brief Load HFP microphone enablement mode from NVS
 * @param enabled  Output: 0=disabled, 1=enabled
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if not saved
 */
esp_err_t config_storage_load_mic_enabled(uint8_t *enabled);

/**
 * @brief Clear all keys in NVS namespace
 */
esp_err_t config_storage_clear_all(void);

esp_err_t config_storage_save_ota_ready(uint8_t ready);
esp_err_t config_storage_load_ota_ready(uint8_t *ready);

esp_err_t config_storage_save_board_model(uint8_t model);
esp_err_t config_storage_load_board_model(uint8_t *model);

/**
 * @brief Save inactivity deep sleep timeout to NVS
 * @param minutes  Idle minutes before entering deep sleep (1-120)
 * @return ESP_OK on success
 */
esp_err_t config_storage_save_sleep_timeout(uint8_t minutes);

/**
 * @brief Load inactivity deep sleep timeout from NVS
 * @param minutes  Output: idle minutes (default 5)
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if not saved
 */
esp_err_t config_storage_load_sleep_timeout(uint8_t *minutes);

#endif /* __CONFIG_STORAGE_H__ */
