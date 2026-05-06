/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BUTTON_HANDLER_H__
#define __BUTTON_HANDLER_H__

/**
 * @brief Initialize button GPIO pins and start debounce task
 */
void button_handler_init(void);

/**
 * @brief Deinitialize button handler
 */
void button_handler_deinit(void);

#endif /* __BUTTON_HANDLER_H__ */
