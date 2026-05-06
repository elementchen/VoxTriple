/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BT_APP_HF_H__
#define __BT_APP_HF_H__

#include <stdint.h>
#include "esp_hf_client_api.h"
#include "esp_bt_defs.h"

extern esp_bd_addr_t hf_peer_addr;

#define BT_HF_APP_TAG  "BT_APP_HF"

/**
 * @brief Callback function for HFP HF Client events
 */
void bt_app_hf_client_cb(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param);

#endif /* __BT_APP_HF_H__ */
