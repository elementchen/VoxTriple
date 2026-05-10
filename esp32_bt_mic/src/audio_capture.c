/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Uses legacy I2S driver (driver/i2s.h) — the new driver (driver/i2s_std.h)
 * produced audible clock jitter / noise with INMP441 at 16kHz. The legacy
 * driver is proven clean on the WiFi sister project.
 */

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "audio_capture.h"

static const char *TAG = "AUDIO_CAPTURE";

static bool s_initialized = false;
static bool s_started = false;

esp_err_t audio_capture_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing I2S (legacy driver) for INMP441");

    i2s_config_t i2s_cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
    };

    i2s_pin_config_t pin_cfg = {
        .bck_io_num = CONFIG_I2S_MIC_BCK_PIN,
        .ws_io_num  = CONFIG_I2S_MIC_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = CONFIG_I2S_MIC_DATA_PIN,
    };

    esp_err_t ret = i2s_driver_install(I2S_NUM_0, &i2s_cfg, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_set_pin(I2S_NUM_0, &pin_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_set_pin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);

    s_initialized = true;
    ESP_LOGI(TAG, "I2S ready: %dHz, bck=%d, ws=%d, din=%d",
             I2S_SAMPLE_RATE,
             CONFIG_I2S_MIC_BCK_PIN, CONFIG_I2S_MIC_WS_PIN, CONFIG_I2S_MIC_DATA_PIN);
    return ESP_OK;
}

esp_err_t audio_capture_start(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (s_started) return ESP_OK;
    s_started = true;
    i2s_zero_dma_buffer(I2S_NUM_0);
    ESP_LOGI(TAG, "I2S capture started");
    return ESP_OK;
}

esp_err_t audio_capture_stop(void)
{
    if (!s_started) return ESP_OK;
    /* Don't call i2s_stop() — the legacy driver's stop/start cycle
     * can leave DMA in a broken state, causing i2s_read() to hang or
     * return garbage on the next SCO connection. Just mark inactive. */
    s_started = false;
    ESP_LOGI(TAG, "I2S capture stopped (DMA kept alive)");
    return ESP_OK;
}

esp_err_t audio_capture_read(uint8_t *buf, size_t buf_len, size_t *bytes_read, uint32_t timeout_ms)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;
    return i2s_read(I2S_NUM_0, buf, buf_len, bytes_read, portMAX_DELAY);
}

void audio_capture_deinit(void)
{
    if (s_initialized) {
        i2s_driver_uninstall(I2S_NUM_0);
        s_initialized = false;
    }
}
