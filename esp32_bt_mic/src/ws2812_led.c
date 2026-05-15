/*
 * SPDX-FileCopyrightText: 2024 VoxTriple Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "ws2812_led.h"

static const char *TAG = "WS2812";

static led_strip_handle_t s_led_strip = NULL;
static TaskHandle_t s_anim_task = NULL;
static volatile bool s_anim_running = false;

/* HSV → RGB helper */
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v,
                       uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region = h / 60;
    uint8_t remainder = (h - region * 60) * 255 / 60;
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
    case 0:  *r = v; *g = t; *b = p; break;
    case 1:  *r = q; *g = v; *b = p; break;
    case 2:  *r = p; *g = v; *b = t; break;
    case 3:  *r = p; *g = q; *b = v; break;
    case 4:  *r = t; *g = p; *b = v; break;
    default: *r = v; *g = p; *b = q; break;
    }
}

/* Rainbow animation task */
static void ws2812_anim_task(void *arg)
{
    uint16_t base_hue = 0;

    ESP_LOGI(TAG, "Rainbow animation started");

    while (s_anim_running) {
        for (int i = 0; i < WS2812_LED_COUNT; i++) {
            /* Each LED offset 45°. Ping-pong in 60-240° to avoid wrap-jump. */
            int raw = (base_hue + i * 45) % 360;
            if (raw > 180) raw = 360 - raw;
            uint16_t hue = 60 + raw;
            uint8_t r, g, b;
            hsv_to_rgb(hue, 255, 64, &r, &g, &b);
            led_strip_set_pixel(s_led_strip, i, r, g, b);
        }
        led_strip_refresh(s_led_strip);

        base_hue = (base_hue + 1) % 360;  /* slow rotation */
        vTaskDelay(pdMS_TO_TICKS(15));    /* ~66fps */
    }

    /* Turn off all LEDs */
    for (int i = 0; i < WS2812_LED_COUNT; i++) {
        led_strip_set_pixel(s_led_strip, i, 0, 0, 0);
    }
    led_strip_refresh(s_led_strip);

    ESP_LOGI(TAG, "Rainbow animation stopped");
    vTaskDelete(NULL);
}

void ws2812_init(void)
{
    led_strip_config_t strip_cfg = {
        .strip_gpio_num = WS2812_GPIO,
        .max_leds = WS2812_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  /* 10 MHz */
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Clear strip on init */
    for (int i = 0; i < WS2812_LED_COUNT; i++) {
        led_strip_set_pixel(s_led_strip, i, 0, 0, 0);
    }
    led_strip_refresh(s_led_strip);

    ESP_LOGI(TAG, "WS2812 initialized: GPIO=%d, %d LEDs", WS2812_GPIO, WS2812_LED_COUNT);
}

void ws2812_rainbow_start(void)
{
    if (s_anim_running) return;
    if (!s_led_strip) {
        ESP_LOGW(TAG, "WS2812 not initialized");
        return;
    }

    s_anim_running = true;
    xTaskCreate(ws2812_anim_task, "ws2812_anim", 4096, NULL, 2, &s_anim_task);
}

void ws2812_rainbow_stop(void)
{
    if (!s_anim_running) return;
    s_anim_running = false;
    /* Let the rainbow task detect the flag and exit on its own.
     * Don't block here — vTaskDelay starves the BT controller. */
    s_anim_task = NULL;
}

void ws2812_deinit(void)
{
    ws2812_rainbow_stop();
    if (s_led_strip) {
        led_strip_del(s_led_strip);
        s_led_strip = NULL;
    }
}
