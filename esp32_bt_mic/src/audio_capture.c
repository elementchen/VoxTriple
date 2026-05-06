/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "audio_capture.h"

static const char *TAG = "AUDIO_CAPTURE";

static i2s_chan_handle_t s_rx_chan = NULL;
static bool s_initialized = false;
static bool s_started = false;

esp_err_t audio_capture_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing I2S for INMP441 microphone");

    /* Step 1: Allocate I2S RX channel */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = 240;

    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &s_rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Step 2: Configure standard mode for INMP441 */
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_MONO
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = CONFIG_I2S_MIC_BCK_PIN,
            .ws   = CONFIG_I2S_MIC_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = CONFIG_I2S_MIC_DATA_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    /* INMP441 uses left channel when L/R pin is tied to GND */
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    ret = i2s_channel_init_std_mode(s_rx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S standard mode: %s", esp_err_to_name(ret));
        i2s_del_channel(s_rx_chan);
        s_rx_chan = NULL;
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "I2S initialized: rate=%d, bits=%d, bck=%d, ws=%d, din=%d",
             I2S_SAMPLE_RATE, I2S_SAMPLE_BITS,
             CONFIG_I2S_MIC_BCK_PIN, CONFIG_I2S_MIC_WS_PIN, CONFIG_I2S_MIC_DATA_PIN);

    return ESP_OK;
}

esp_err_t audio_capture_start(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_started) {
        ESP_LOGW(TAG, "Already started");
        return ESP_OK;
    }

    esp_err_t ret = i2s_channel_enable(s_rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    s_started = true;
    ESP_LOGI(TAG, "I2S capture started");
    return ESP_OK;
}

esp_err_t audio_capture_stop(void)
{
    if (!s_started || !s_rx_chan) {
        return ESP_OK;
    }

    esp_err_t ret = i2s_channel_disable(s_rx_chan);
    s_started = false;
    ESP_LOGI(TAG, "I2S capture stopped");
    return ret;
}

esp_err_t audio_capture_read(uint8_t *buf, size_t buf_len, size_t *bytes_read, uint32_t timeout_ms)
{
    if (!s_started || !s_rx_chan) {
        return ESP_ERR_INVALID_STATE;
    }

    return i2s_channel_read(s_rx_chan, buf, buf_len, bytes_read, timeout_ms);
}

void audio_capture_deinit(void)
{
    if (s_started) {
        audio_capture_stop();
    }

    if (s_rx_chan) {
        i2s_del_channel(s_rx_chan);
        s_rx_chan = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "I2S deinitialized");
}
