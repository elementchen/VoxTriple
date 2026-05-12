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
 * @brief Blink all LEDs red N times (200ms on/off each).
 *        Blocking call — runs in caller's task context.
 */
void ws2812_blink_red(int count);

/**
 * @brief Set all LEDs to a solid color for `duration_ms`.
 *        Non-blocking — starts a background timer.
 */
void ws2812_solid_color(uint8_t r, uint8_t g, uint8_t b, int duration_ms);

/**
 * @brief Blink all LEDs at `interval_ms` endlessly.
 *        Call ws2812_rainbow_stop() or ws2812_blink_stop() to stop.
 */
void ws2812_blink_color(uint8_t r, uint8_t g, uint8_t b, int interval_ms);

/**
 * @brief Stop blink mode and turn off LEDs.
 */
void ws2812_blink_stop(void);

/**
 * @brief Deinitialize WS2812 driver
 */
void ws2812_deinit(void);

#endif /* __WS2812_LED_H__ */
