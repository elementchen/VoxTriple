/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BT_HFP_HF_H__
#define __BT_HFP_HF_H__

#include "esp_hf_client_api.h"

#define BT_HFP_TAG  "BT_HFP_HF"

/**
 * @brief Start HFP HF audio streaming (called when audio connects)
 */
void bt_hfp_hf_audio_start(void);

/**
 * @brief Stop HFP HF audio streaming (called when audio disconnects)
 */
void bt_hfp_hf_audio_stop(void);

/**
 * @brief PTT press: request SCO audio connection to AG
 */
void bt_hfp_hf_ptt_press(void);

/**
 * @brief PTT release: disconnect SCO audio from AG
 */
void bt_hfp_hf_ptt_release(void);

#endif /* __BT_HFP_HF_H__ */
