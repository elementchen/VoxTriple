/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_task.h"
#include "audio_capture.h"
#include "bt_hfp_ag.h"
#include "bt_init.h"

static const char *TAG = "AUDIO_TASK";

#define AUDIO_TASK_STACK_SIZE  4096
#define AUDIO_TASK_PRIORITY    5
#define AUDIO_READ_SIZE        2048

static TaskHandle_t s_audio_task_handle = NULL;
static bool s_audio_task_running = false;

/**
 * @brief Audio processing task
 *        Continuously reads I2S data and feeds it to HFP AG ring buffer
 */
static void audio_task_func(void *arg)
{
    uint8_t *read_buf = (uint8_t *)calloc(1, AUDIO_READ_SIZE);
    assert(read_buf);

    ESP_LOGI(TAG, "Audio task started");

    while (s_audio_task_running) {
        /* Only feed audio when HFP SCO audio is active */
        if (!bt_audio_is_active()) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        size_t bytes_read = 0;
        esp_err_t ret = audio_capture_read(read_buf, AUDIO_READ_SIZE, &bytes_read, 100);

        if (ret == ESP_OK && bytes_read > 0) {
            /* Resample if needed: INMP441 runs at 16kHz, HFP CVSD expects 8kHz
             * For mSBC (WBS), 16kHz is correct.
             * Simple approach: if CVSD mode, downsample by taking every other sample.
             * For now, pass 16kHz data directly (works for WBS/mSBC mode). */
            bt_hfp_feed_audio(read_buf, bytes_read);
        } else if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    free(read_buf);
    ESP_LOGI(TAG, "Audio task stopped");
    vTaskDelete(NULL);
}

void audio_task_start(void)
{
    if (s_audio_task_running) {
        ESP_LOGW(TAG, "Audio task already running");
        return;
    }

    s_audio_task_running = true;
    xTaskCreate(audio_task_func, "AudioTask", AUDIO_TASK_STACK_SIZE,
                NULL, AUDIO_TASK_PRIORITY, &s_audio_task_handle);
    ESP_LOGI(TAG, "Audio task created");
}

void audio_task_stop(void)
{
    if (!s_audio_task_running) {
        return;
    }

    ESP_LOGI(TAG, "Stopping audio task");
    s_audio_task_running = false;

    /* Wait for task to finish */
    if (s_audio_task_handle) {
        /* Give the task time to exit its loop */
        vTaskDelay(pdMS_TO_TICKS(200));
        s_audio_task_handle = NULL;
    }
}
