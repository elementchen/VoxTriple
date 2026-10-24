/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef BOARD_PROFILE_H
#define BOARD_PROFILE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOARD_MODEL_WEMOS_18650 = 0,
    BOARD_MODEL_ESP32_LITE_V1 = 1,
    BOARD_MODEL_MAX
} board_model_t;

typedef struct {
    board_model_t model;
    const char *model_name;
    gpio_num_t led_gpio;
    gpio_num_t i2s_bck_gpio;
    gpio_num_t i2s_ws_gpio;
    gpio_num_t i2s_data_gpio;
    gpio_num_t btn_gpios[4];
} board_profile_t;

/**
 * @brief Initialize board profile subsystem, loads model from NVS (default: ESP32_LITE_V1)
 */
void board_profile_init(void);

/**
 * @brief Get currently active board profile
 */
const board_profile_t *board_profile_get_current(void);

/**
 * @brief Set board model and save to NVS
 */
esp_err_t board_profile_set_model(board_model_t model);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_PROFILE_H */
