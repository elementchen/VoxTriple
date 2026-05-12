/*
 * SPDX-FileCopyrightText: 2024 VoxTriple Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __WS2812_LED_H__
#define __WS2812_LED_H__

#include <stdbool.h>

#define WS2812_GPIO        CONFIG_WS2812_GPIO
#define WS2812_LED_COUNT   15

/**
 * @brief Initialize WS2812 LED strip (RMT driver)
 */
void ws2812_init(void);

/**
 * @brief Start rainbow animation (called on PTT press when BLE connected)
 */
void ws2812_rainbow_start(void);

/**
 * @brief Stop animation and turn off all LEDs
 */
void ws2812_rainbow_stop(void);

/**
 * @brief Deinitialize WS2812 driver
 */
void ws2812_deinit(void);

#endif /* __WS2812_LED_H__ */
