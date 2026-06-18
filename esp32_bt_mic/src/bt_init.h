/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BT_INIT_H__
#define __BT_INIT_H__

#include <stdbool.h>
#include "esp_err.h"

extern char g_bt_device_name[];

esp_err_t bt_nvs_init(void);
esp_err_t bt_stack_init(void);

bool bt_hfp_is_connected(void);
bool bt_audio_is_active(void);
void bt_hfp_set_connected(bool connected);
void bt_audio_set_active(bool active);
void bt_hfp_hf_wake_acl(void);

#endif /* __BT_INIT_H__ */
