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

#define NVS_NAMESPACE  "bt_mic_cfg"
#define BUTTON_NUM     3

/* NVS key names for button mappings */
#define NVS_KEY_BTN1_MAP  "btn1_map"
#define NVS_KEY_BTN2_MAP  "btn2_map"
#define NVS_KEY_BTN3_MAP  "btn3_map"

/**
 * @brief Initialize NVS configuration storage
 */
void config_storage_init(void);

esp_err_t config_storage_save_button(uint8_t button_id, uint8_t vk_code, uint8_t modifier);
esp_err_t config_storage_load_button(uint8_t button_id, uint8_t *vk_code, uint8_t *modifier);

#endif /* __CONFIG_STORAGE_H__ */
