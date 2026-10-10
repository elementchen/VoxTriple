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

/**
 * @brief Forget paired Bluetooth devices, clear link keys and auto-reconnect MAC, then reboot
 */
void system_reset_bt_pairing(void);

/**
 * @brief Dynamically update inactivity sleep timeout without reboot
 */
void button_handler_update_sleep_timeout(uint8_t timeout_min);

#endif /* __BUTTON_HANDLER_H__ */
