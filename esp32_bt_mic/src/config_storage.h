/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __CONFIG_STORAGE_H__
#define __CONFIG_STORAGE_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#include "esp_bt_defs.h"

#define NVS_NAMESPACE  "bt_mic_cfg"
#define BUTTON_NUM     3
#define MAX_DEVICES    2

/* NVS device list keys */
#define NVS_KEY_DEV_COUNT   "dev_cnt"
#define NVS_KEY_ACTIVE_DEV  "act_dev"
#define NVS_KEY_DEV_0       "dev_0"
#define NVS_KEY_DEV_1       "dev_1"

/**
 * @brief Initialize NVS configuration storage
 */
void config_storage_init(void);

/* NVS key names for button mappings */
#define NVS_KEY_BTN1_MAP  "btn1_map"
#define NVS_KEY_BTN2_MAP  "btn2_map"
#define NVS_KEY_BTN3_MAP  "btn3_map"

/* Button mapping (unchanged) */
esp_err_t config_storage_save_button(uint8_t button_id, uint8_t vk_code, uint8_t modifier);
esp_err_t config_storage_load_button(uint8_t button_id, uint8_t *vk_code, uint8_t *modifier);

/**
 * @brief Save a peer device address to slot `idx` (0 or 1).
 *        Returns ESP_OK (overwrites if slot already occupied).
 */
esp_err_t config_storage_save_device(int idx, const esp_bd_addr_t addr);

/**
 * @brief Load peer address from slot `idx`. Returns ESP_ERR_NVS_NOT_FOUND if empty.
 */
esp_err_t config_storage_load_device(int idx, esp_bd_addr_t addr);

/**
 * @brief Get number of stored devices.
 */
int config_storage_get_device_count(void);

/**
 * @brief Get / set active device index (0 or 1).
 */
int  config_storage_get_active_device(void);
void config_storage_set_active_device(int idx);

/**
 * @brief Clear (erase) a device slot.
 */
void config_storage_clear_device(int idx);

#endif /* __CONFIG_STORAGE_H__ */
