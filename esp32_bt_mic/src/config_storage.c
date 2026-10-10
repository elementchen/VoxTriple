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
    0x09,  /* VK_TAB (Tab) */
};

static const uint8_t s_default_mod[BUTTON_NUM] = {
    0x00,  /* No modifier */
    0x00,
    0x00,
    0x00,
};

static const char *s_nvs_keys[BUTTON_NUM] = {
    NVS_KEY_BTN1_MAP,
    NVS_KEY_BTN2_MAP,
    NVS_KEY_BTN3_MAP,
    NVS_KEY_BTN4_MAP,
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

    /* Initialize sleep_mode default configuration (default to 1) */
    uint8_t sleep_mode = 0;
    if (config_storage_load_sleep_mode(&sleep_mode) != ESP_OK) {
        ESP_LOGI(TAG, "Sleep mode: no saved config, setting default (enabled)");
        config_storage_save_sleep_mode(1);
    }

    /* Initialize mic_enabled default configuration (default to 1) */
    uint8_t mic_enabled = 0;
    if (config_storage_load_mic_enabled(&mic_enabled) != ESP_OK) {
        ESP_LOGI(TAG, "Mic enabled: no saved config, setting default (enabled)");
        config_storage_save_mic_enabled(1);
    }
}

static const char *s_slot0_keys[BUTTON_NUM] = {
    "s0_btn1", "s0_btn2", "s0_btn3", "s0_btn4"
};
static const char *s_slot1_keys[BUTTON_NUM] = {
    "s1_btn1", "s1_btn2", "s1_btn3", "s1_btn4"
};

esp_err_t config_storage_save_slot_button(uint8_t slot, uint8_t button_id, uint8_t vk_code, uint8_t modifier)
{
    if (slot > 1 || button_id >= BUTTON_NUM) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t data[2] = {vk_code, modifier};
    const char *key = (slot == 0) ? s_slot0_keys[button_id] : s_slot1_keys[button_id];
    ret = nvs_set_blob(nvs_handle, key, data, sizeof(data));

    /* 向后兼容：若是 slot 0 则同时保存一份到传统 btnX_map */
    if (slot == 0 && ret == ESP_OK) {
        nvs_set_blob(nvs_handle, s_nvs_keys[button_id], data, sizeof(data));
    }

    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS save slot %d button %d failed: %s", slot, button_id + 1, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Saved slot %d button %d mapping: VK=0x%02X, MOD=0x%02X",
                 slot, button_id + 1, vk_code, modifier);
    }

    return ret;
}

esp_err_t config_storage_load_slot_button(uint8_t slot, uint8_t button_id, uint8_t *vk_code, uint8_t *modifier)
{
    if (slot > 1 || button_id >= BUTTON_NUM || !vk_code || !modifier) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        *vk_code = s_default_vk[button_id];
        *modifier = s_default_mod[button_id];
        return ret;
    }

    uint8_t data[2] = {0, 0};
    size_t length = sizeof(data);
    const char *key = (slot == 0) ? s_slot0_keys[button_id] : s_slot1_keys[button_id];

    ret = nvs_get_blob(nvs_handle, key, data, &length);
    /* 向后兼容：如果是 slot 0 且未找到，尝试读取传统 btnX_map */
    if (ret != ESP_OK && slot == 0) {
        length = sizeof(data);
        ret = nvs_get_blob(nvs_handle, s_nvs_keys[button_id], data, &length);
    }

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

esp_err_t config_storage_save_button(uint8_t button_id, uint8_t vk_code, uint8_t modifier)
{
    uint8_t slot = 0;
    config_storage_get_active_slot(&slot);
    return config_storage_save_slot_button(slot, button_id, vk_code, modifier);
}

esp_err_t config_storage_load_button(uint8_t button_id, uint8_t *vk_code, uint8_t *modifier)
{
    uint8_t slot = 0;
    config_storage_get_active_slot(&slot);
    return config_storage_load_slot_button(slot, button_id, vk_code, modifier);
}

#define NVS_KEY_HFP_ADDR     "hfp_addr"
#define NVS_KEY_ACTIVE_SLOT  "act_slot"
#define NVS_KEY_HFP_ADDR_0   "hfp_addr_0"
#define NVS_KEY_HFP_ADDR_1   "hfp_addr_1"

esp_err_t config_storage_get_active_slot(uint8_t *slot)
{
    if (!slot) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        *slot = 0;
        return ESP_OK;
    }
    uint8_t val = 0;
    ret = nvs_get_u8(nvs_handle, NVS_KEY_ACTIVE_SLOT, &val);
    nvs_close(nvs_handle);
    if (ret == ESP_OK && val <= 1) {
        *slot = val;
    } else {
        *slot = 0;
    }
    return ESP_OK;
}

esp_err_t config_storage_set_active_slot(uint8_t slot)
{
    if (slot > 1) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_u8(nvs_handle, NVS_KEY_ACTIVE_SLOT, slot);
    if (ret == ESP_OK) nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Active host slot set to: %d", slot);
    }
    return ret;
}

esp_err_t config_storage_load_slot_hfp_addr(uint8_t slot, esp_bd_addr_t addr)
{
    if (slot > 1 || !addr) return ESP_ERR_INVALID_ARG;
    const char *key = (slot == 0) ? NVS_KEY_HFP_ADDR_0 : NVS_KEY_HFP_ADDR_1;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) return ret;

    size_t len = ESP_BD_ADDR_LEN;
    ret = nvs_get_blob(nvs_handle, key, addr, &len);
    // 向后兼容：如果是 Slot 0 且未找到 hfp_addr_0，尝试读取原有的 hfp_addr
    if (ret != ESP_OK && slot == 0) {
        len = ESP_BD_ADDR_LEN;
        ret = nvs_get_blob(nvs_handle, NVS_KEY_HFP_ADDR, addr, &len);
    }
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t config_storage_save_slot_hfp_addr(uint8_t slot, const esp_bd_addr_t addr)
{
    if (slot > 1 || !addr) return ESP_ERR_INVALID_ARG;
    const char *key = (slot == 0) ? NVS_KEY_HFP_ADDR_0 : NVS_KEY_HFP_ADDR_1;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_blob(nvs_handle, key, addr, ESP_BD_ADDR_LEN);
    // 同时同步保存到全局 hfp_addr 保证遗留模块兼容
    if (slot == 0 && ret == ESP_OK) {
        nvs_set_blob(nvs_handle, NVS_KEY_HFP_ADDR, addr, ESP_BD_ADDR_LEN);
    }
    if (ret == ESP_OK) nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Slot %d HFP addr saved: %02x:%02x:%02x:%02x:%02x:%02x",
                 slot, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    }
    return ret;
}

#define NVS_KEY_S0_OS      "s0_os"
#define NVS_KEY_S1_OS      "s1_os"

esp_err_t config_storage_save_slot_os_type(uint8_t slot, uint8_t os_type)
{
    if (slot > 1) return ESP_ERR_INVALID_ARG;
    const char *key = (slot == 0) ? NVS_KEY_S0_OS : NVS_KEY_S1_OS;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_u8(nvs_handle, key, os_type);
    if (ret == ESP_OK) nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Slot %d OS type saved: %d (%s)", slot, os_type, 
             (os_type == OS_TYPE_APPLE) ? "Apple" : (os_type == OS_TYPE_WINDOWS) ? "Windows" : "Unknown");
    return ret;
}

esp_err_t config_storage_load_slot_os_type(uint8_t slot, uint8_t *os_type)
{
    if (slot > 1 || !os_type) return ESP_ERR_INVALID_ARG;
    const char *key = (slot == 0) ? NVS_KEY_S0_OS : NVS_KEY_S1_OS;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        *os_type = OS_TYPE_UNKNOWN;
        return ret;
    }

    ret = nvs_get_u8(nvs_handle, key, os_type);
    nvs_close(nvs_handle);
    if (ret != ESP_OK) {
        *os_type = OS_TYPE_UNKNOWN;
    }
    return ret;
}

esp_err_t config_storage_clear_slot_hfp_addr(uint8_t slot)
{
    if (slot > 1) return ESP_ERR_INVALID_ARG;
    const char *key = (slot == 0) ? NVS_KEY_HFP_ADDR_0 : NVS_KEY_HFP_ADDR_1;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    nvs_erase_key(nvs_handle, key);
    if (slot == 0) {
        nvs_erase_key(nvs_handle, NVS_KEY_HFP_ADDR);
    }
    const char *os_key = (slot == 0) ? NVS_KEY_S0_OS : NVS_KEY_S1_OS;
    nvs_erase_key(nvs_handle, os_key);

    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Slot %d HFP address and OS type cleared from NVS.", slot);
    return ESP_OK;
}

esp_err_t config_storage_save_hfp_addr(esp_bd_addr_t addr)
{
    uint8_t slot = 0;
    config_storage_get_active_slot(&slot);
    return config_storage_save_slot_hfp_addr(slot, addr);
}

esp_err_t config_storage_load_hfp_addr(esp_bd_addr_t addr)
{
    uint8_t slot = 0;
    config_storage_get_active_slot(&slot);
    return config_storage_load_slot_hfp_addr(slot, addr);
}

esp_err_t config_storage_clear_hfp_addr(void)
{
    uint8_t slot = 0;
    config_storage_get_active_slot(&slot);
    return config_storage_clear_slot_hfp_addr(slot);
}

bool config_storage_is_slot_paired(uint8_t slot)
{
    if (slot > 1) return false;
    esp_bd_addr_t addr;
    if (config_storage_load_slot_hfp_addr(slot, addr) != ESP_OK) {
        return false;
    }
    for (int i = 0; i < ESP_BD_ADDR_LEN; i++) {
        if (addr[i] != 0) return true;
    }
    return false;
}

#define NVS_KEY_S0_SLEEP   "s0_sleep"
#define NVS_KEY_S1_SLEEP   "s1_sleep"
#define NVS_KEY_S0_TMOUT   "s0_tmout"
#define NVS_KEY_S1_TMOUT   "s1_tmout"
#define NVS_KEY_S0_MIC     "s0_mic"
#define NVS_KEY_S1_MIC     "s1_mic"

esp_err_t config_storage_save_slot_sleep_mode(uint8_t slot, uint8_t enabled)
{
    if (slot > 1) return ESP_ERR_INVALID_ARG;
    const char *key = (slot == 0) ? NVS_KEY_S0_SLEEP : NVS_KEY_S1_SLEEP;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_u8(nvs_handle, key, enabled);
    if (slot == 0 && ret == ESP_OK) {
        nvs_set_u8(nvs_handle, NVS_KEY_SLEEP_MODE, enabled);
    }
    if (ret == ESP_OK) nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t config_storage_load_slot_sleep_mode(uint8_t slot, uint8_t *enabled)
{
    if (slot > 1 || !enabled) return ESP_ERR_INVALID_ARG;
    const char *key = (slot == 0) ? NVS_KEY_S0_SLEEP : NVS_KEY_S1_SLEEP;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        *enabled = 1;
        return ret;
    }

    ret = nvs_get_u8(nvs_handle, key, enabled);
    if (ret != ESP_OK && slot == 0) {
        ret = nvs_get_u8(nvs_handle, NVS_KEY_SLEEP_MODE, enabled);
    }
    nvs_close(nvs_handle);
    if (ret != ESP_OK) {
        *enabled = 1;
    }
    return ret;
}

esp_err_t config_storage_save_slot_sleep_timeout(uint8_t slot, uint8_t minutes)
{
    if (slot > 1) return ESP_ERR_INVALID_ARG;
    const char *key = (slot == 0) ? NVS_KEY_S0_TMOUT : NVS_KEY_S1_TMOUT;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_u8(nvs_handle, key, minutes);
    if (slot == 0 && ret == ESP_OK) {
        nvs_set_u8(nvs_handle, NVS_KEY_SLEEP_TIMEOUT, minutes);
    }
    if (ret == ESP_OK) nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t config_storage_load_slot_sleep_timeout(uint8_t slot, uint8_t *minutes)
{
    if (slot > 1 || !minutes) return ESP_ERR_INVALID_ARG;
    const char *key = (slot == 0) ? NVS_KEY_S0_TMOUT : NVS_KEY_S1_TMOUT;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        *minutes = 5;
        return ret;
    }

    ret = nvs_get_u8(nvs_handle, key, minutes);
    if (ret != ESP_OK && slot == 0) {
        ret = nvs_get_u8(nvs_handle, NVS_KEY_SLEEP_TIMEOUT, minutes);
    }
    nvs_close(nvs_handle);
    if (ret != ESP_OK) {
        *minutes = 5;
    }
    return ret;
}

esp_err_t config_storage_save_slot_mic_enabled(uint8_t slot, uint8_t enabled)
{
    if (slot > 1) return ESP_ERR_INVALID_ARG;
    const char *key = (slot == 0) ? NVS_KEY_S0_MIC : NVS_KEY_S1_MIC;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_u8(nvs_handle, key, enabled);
    if (slot == 0 && ret == ESP_OK) {
        nvs_set_u8(nvs_handle, NVS_KEY_MIC_ENABLED, enabled);
    }
    if (ret == ESP_OK) nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t config_storage_load_slot_mic_enabled(uint8_t slot, uint8_t *enabled)
{
    if (slot > 1 || !enabled) return ESP_ERR_INVALID_ARG;
    const char *key = (slot == 0) ? NVS_KEY_S0_MIC : NVS_KEY_S1_MIC;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        *enabled = 1;
        return ret;
    }

    ret = nvs_get_u8(nvs_handle, key, enabled);
    if (ret != ESP_OK && slot == 0) {
        ret = nvs_get_u8(nvs_handle, NVS_KEY_MIC_ENABLED, enabled);
    }
    nvs_close(nvs_handle);
    if (ret != ESP_OK) {
        *enabled = 1;
    }
    return ret;
}

#define NVS_KEY_TX_POWER   "tx_power"
#define NVS_KEY_SLEEP_MODE "sleep_mode"

esp_err_t config_storage_save_tx_power(uint8_t level)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_u8(nvs_handle, NVS_KEY_TX_POWER, level);
    if (ret == ESP_OK) nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "TX power level saved: %d", level);
    }
    return ret;
}

esp_err_t config_storage_load_tx_power(uint8_t *level)
{
    if (!level) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_get_u8(nvs_handle, NVS_KEY_TX_POWER, level);
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t config_storage_save_sleep_mode(uint8_t enabled)
{
    uint8_t slot = 0;
    config_storage_get_active_slot(&slot);
    return config_storage_save_slot_sleep_mode(slot, enabled);
}

esp_err_t config_storage_load_sleep_mode(uint8_t *enabled)
{
    uint8_t slot = 0;
    config_storage_get_active_slot(&slot);
    return config_storage_load_slot_sleep_mode(slot, enabled);
}

esp_err_t config_storage_save_mic_enabled(uint8_t enabled)
{
    uint8_t slot = 0;
    config_storage_get_active_slot(&slot);
    return config_storage_save_slot_mic_enabled(slot, enabled);
}

esp_err_t config_storage_load_mic_enabled(uint8_t *enabled)
{
    uint8_t slot = 0;
    config_storage_get_active_slot(&slot);
    return config_storage_load_slot_mic_enabled(slot, enabled);
}

esp_err_t config_storage_clear_all(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_erase_all(nvs_handle);
    if (ret == ESP_OK) nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Config storage cleared successfully.");
    return ret;
}

esp_err_t config_storage_save_ota_ready(uint8_t ready)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_u8(nvs_handle, NVS_KEY_OTA_READY, ready);
    if (ret == ESP_OK) nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Ready flag saved: %d", ready);
    }
    return ret;
}

esp_err_t config_storage_load_ota_ready(uint8_t *ready)
{
    if (!ready) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_get_u8(nvs_handle, NVS_KEY_OTA_READY, ready);
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t config_storage_save_board_model(uint8_t model)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_u8(nvs_handle, NVS_KEY_BOARD_MODEL, model);
    if (ret == ESP_OK) nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Board model saved: %d", model);
    }
    return ret;
}

esp_err_t config_storage_load_board_model(uint8_t *model)
{
    if (!model) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) return ret;

    ret = nvs_get_u8(nvs_handle, NVS_KEY_BOARD_MODEL, model);
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t config_storage_save_sleep_timeout(uint8_t minutes)
{
    uint8_t slot = 0;
    config_storage_get_active_slot(&slot);
    return config_storage_save_slot_sleep_timeout(slot, minutes);
}

esp_err_t config_storage_load_sleep_timeout(uint8_t *minutes)
{
    uint8_t slot = 0;
    config_storage_get_active_slot(&slot);
    return config_storage_load_slot_sleep_timeout(slot, minutes);
}
