/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * USB Serial OTA — receive firmware via UART, write to OTA partition.
 * Protocol:  "OTA:START:<size>\n" → "OTA:OK\n" → [binary stream] → "OTA:END\n"
 */

#ifndef __UART_OTA_H__
#define __UART_OTA_H__

#include "esp_err.h"

/**
 * @brief Start the UART OTA listener task.
 *        Monitors UART0 for "OTA:START" command.
 */
void uart_ota_init(void);

#endif /* __UART_OTA_H__ */
