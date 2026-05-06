/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __AUDIO_CAPTURE_H__
#define __AUDIO_CAPTURE_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* I2S configuration constants */
#define I2S_SAMPLE_RATE      16000
#define I2S_SAMPLE_BITS      16
#define I2S_CHANNEL_COUNT    1
#define I2S_READ_BUF_SIZE    2048

/**
 * @brief Initialize I2S driver for INMP441 microphone
 * @return ESP_OK on success
 */
esp_err_t audio_capture_init(void);

/**
 * @brief Start I2S reading
 * @return ESP_OK on success
 */
esp_err_t audio_capture_start(void);

/**
 * @brief Stop I2S reading
 * @return ESP_OK on success
 */
esp_err_t audio_capture_stop(void);

/**
 * @brief Read audio data from I2S
 * @param buf     Output buffer
 * @param buf_len Buffer size in bytes
 * @param bytes_read  Actual bytes read
 * @param timeout_ms  Timeout in milliseconds
 * @return ESP_OK on success
 */
esp_err_t audio_capture_read(uint8_t *buf, size_t buf_len, size_t *bytes_read, uint32_t timeout_ms);

/**
 * @brief Deinitialize I2S driver
 */
void audio_capture_deinit(void);

#endif /* __AUDIO_CAPTURE_H__ */
