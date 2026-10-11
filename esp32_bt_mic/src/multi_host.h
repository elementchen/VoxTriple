#ifndef MULTI_HOST_H
#define MULTI_HOST_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 物理两档拨动开关引脚定义：GPIO 33 (左侧排针 Pin 8，接地为 Pair 2，悬空为 Pair 1) */
#define MULTI_HOST_SWITCH_GPIO  GPIO_NUM_33

/**
 * @brief Initialize multi-host module and physical slide switch (GPIO 33)
 */
esp_err_t multi_host_init(void);

/**
 * @brief Get currently active host slot (0 or 1)
 */
uint8_t multi_host_get_current_slot(void);

/**
 * @brief Read hardware switch state directly from GPIO 33
 * @return 0 for Pair 1 (High/Floating), 1 for Pair 2 (Low/GND)
 */
uint8_t multi_host_read_hardware_switch_slot(void);

/**
 * @brief Trigger switching to the other host slot (Slot 0 <-> Slot 1)
 *        Gracefully disconnects current host, saves new slot, and initiates reconnect/discovery
 */
esp_err_t multi_host_switch_slot(void);

/**
 * @brief Switch explicitly to target host slot (0 or 1)
 */
esp_err_t multi_host_set_slot(uint8_t slot);

/**
 * @brief Trigger physical LED feedback pattern for the active slot
 *        Slot 0: 1 flash x 3 times
 *        Slot 1: 2 flashes x 3 times
 */
void multi_host_trigger_led_feedback(uint8_t slot);

#ifdef __cplusplus
}
#endif

#endif // MULTI_HOST_H
