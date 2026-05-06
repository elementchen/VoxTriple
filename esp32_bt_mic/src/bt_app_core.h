/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __BT_APP_CORE_H__
#define __BT_APP_CORE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define BT_APP_CORE_TAG  "BT_APP_CORE"

#define BT_APP_SIG_WORK_DISPATCH  (0x01)

/**
 * @brief Handler for the dispatched work
 */
typedef void (*bt_app_cb_t)(uint16_t event, void *param);

/* Message to be sent */
typedef struct {
    uint16_t      sig;      /*!< signal to bt_app_task */
    uint16_t      event;    /*!< message event id */
    bt_app_cb_t   cb;       /*!< context switch callback */
    void         *param;    /*!< parameter area needs to be last */
} bt_app_msg_t;

/**
 * @brief Parameter deep-copy function to be customized
 */
typedef void (*bt_app_copy_cb_t)(bt_app_msg_t *msg, void *p_dest, void *p_src);

/**
 * @brief Work dispatcher for the application task
 */
bool bt_app_work_dispatch(bt_app_cb_t p_cback, uint16_t event, void *p_params, int param_len, bt_app_copy_cb_t p_copy_cback);

/**
 * @brief Start the BT application task
 */
void bt_app_task_start_up(void);

/**
 * @brief Stop the BT application task
 */
void bt_app_task_shut_down(void);

#endif /* __BT_APP_CORE_H__ */
