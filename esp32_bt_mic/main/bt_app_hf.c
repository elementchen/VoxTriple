/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include "esp_hf_ag_api.h"
#include "esp_bt_defs.h"

/* Peer device address - shared between HFP AG callback and main */
esp_bd_addr_t hf_peer_addr;
