/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BT_HFP_AG_H__
#define __BT_HFP_AG_H__

#include "esp_hf_ag_api.h"

#define BT_HFP_TAG  "BT_HFP_AG"

/**
 * @brief Callback for HFP AG events
 */
void bt_hfp_ag_cb(esp_hf_cb_event_t event, esp_hf_cb_param_t *param);

/**
 * @brief Start HFP AG audio data streaming (called when SCO connects)
 */
void bt_hfp_audio_start(void);

/**
 * @brief Stop HFP AG audio data streaming (called when SCO disconnects)
 */
void bt_hfp_audio_stop(void);

/**
 * @brief Feed PCM audio data into the HFP outgoing ring buffer
 * @param data  Pointer to PCM data
 * @param len   Length in bytes
 */
void bt_hfp_feed_audio(const uint8_t *data, uint32_t len);

#endif /* __BT_HFP_AG_H__ */
