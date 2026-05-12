/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "config_storage.h"

static const char *TAG = "CONFIG_STORAGE";

/* Default button mappings (Windows VK codes) */
static const uint8_t s_default_vk[BUTTON_NUM] = {
    0x0D,  /* VK_RETURN (Enter) */
    0x1B,  /* VK_ESCAPE (Esc) */
    0x20,  /* VK_SPACE (Space) */
};

static const uint8_t s_default_mod[BUTTON_NUM] = {
    0x00,  /* No modifier */
    0x00,
    0x00,
};

static const char *s_nvs_keys[BUTTON_NUM] = {
    NVS_KEY_BTN1_MAP,
    NVS_KEY_BTN2_MAP,
    NVS_KEY_BTN3_MAP,
};

void config_storage_init(void)
{
    ESP_LOGI(TAG, "Initializing configuration storage");

    /* Try to load each button mapping; if not found, save defaults */
    for (int i = 0; i < BUTTON_NUM; i++) {
        uint8_t vk_code = 0, modifier = 0;
        esp_err_t ret = config_storage_load_button(i, &vk_code, &modifier);
        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "Button %d: no saved config, setting defaults (VK=0x%02X, MOD=0x%02X)",
                     i + 1, s_default_vk[i], s_default_mod[i]);
            config_storage_save_button(i, s_default_vk[i], s_default_mod[i]);
        } else {
            ESP_LOGI(TAG, "Button %d: loaded config (VK=0x%02X, MOD=0x%02X)",
                     i + 1, vk_code, modifier);
        }
    }
}

esp_err_t config_storage_save_button(uint8_t button_id, uint8_t vk_code, uint8_t modifier)
{
    if (button_id >= BUTTON_NUM) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Store as 2-byte blob: [vk_code, modifier] */
    uint8_t data[2] = {vk_code, modifier};
    ret = nvs_set_blob(nvs_handle, s_nvs_keys[button_id], data, sizeof(data));

    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS save button %d failed: %s", button_id + 1, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Saved button %d mapping: VK=0x%02X, MOD=0x%02X",
                 button_id + 1, vk_code, modifier);
    }

    return ret;
}

esp_err_t config_storage_load_button(uint8_t button_id, uint8_t *vk_code, uint8_t *modifier)
{
    if (button_id >= BUTTON_NUM || !vk_code || !modifier) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t data[2] = {0, 0};
    size_t length = sizeof(data);

    ret = nvs_get_blob(nvs_handle, s_nvs_keys[button_id], data, &length);
    if (ret == ESP_OK && length == sizeof(data)) {
        *vk_code = data[0];
        *modifier = data[1];
    } else {
        *vk_code = s_default_vk[button_id];
        *modifier = s_default_mod[button_id];
    }

    nvs_close(nvs_handle);
    return ret;
}

