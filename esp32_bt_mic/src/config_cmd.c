#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "cJSON.h"
#include "config_storage.h"
#include "config_cmd.h"
#include "uart_console.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_profile.h"
#include "button_handler.h"
#include "esp_gap_bt_api.h"
#include "multi_host.h"

#define TAG "CONFIG_CMD"

void execute_config_cmd(const char *cmd_line, size_t len, cmd_respond_cb_t respond_cb)
{
    if (!respond_cb) {
        return;
    }

    cJSON *root = cJSON_ParseWithLength(cmd_line, len);
    if (!root) {
        ESP_LOGE(TAG, "JSON Parse failed: %.*s", (int)len, cmd_line);
        return;
    }
    
    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
    if (cmd_item && cJSON_IsString(cmd_item)) {
        const char *cmd = cmd_item->valuestring;
        
        if (strcmp(cmd, "get_config") == 0) {
            uint8_t active_slot = multi_host_get_current_slot();
            uint8_t query_slot = active_slot;
            cJSON *slot_item = cJSON_GetObjectItem(root, "slot");
            if (slot_item && cJSON_IsNumber(slot_item) && slot_item->valueint <= 1) {
                query_slot = slot_item->valueint;
            }

            uint8_t vk1, mod1, vk2, mod2, vk3, mod3, vk4, mod4;
            config_storage_load_slot_button(query_slot, 0, &vk1, &mod1);
            config_storage_load_slot_button(query_slot, 1, &vk2, &mod2);
            config_storage_load_slot_button(query_slot, 2, &vk3, &mod3);
            config_storage_load_slot_button(query_slot, 3, &vk4, &mod4);
            
            uint8_t tx_power = 7;
            config_storage_load_tx_power(&tx_power);
            uint8_t sleep_mode = 0;
            config_storage_load_slot_sleep_mode(query_slot, &sleep_mode);
            uint8_t mic_enabled = 1;
            config_storage_load_slot_mic_enabled(query_slot, &mic_enabled);
            uint8_t sleep_timeout_min = 5;
            config_storage_load_slot_sleep_timeout(query_slot, &sleep_timeout_min);
            
            uint8_t slot0_paired = config_storage_is_slot_paired(0) ? 1 : 0;
            uint8_t slot1_paired = config_storage_is_slot_paired(1) ? 1 : 0;

            uint8_t slot0_os = OS_TYPE_UNKNOWN;
            uint8_t slot1_os = OS_TYPE_UNKNOWN;
            config_storage_load_slot_os_type(0, &slot0_os);
            config_storage_load_slot_os_type(1, &slot1_os);
            uint8_t query_os = (query_slot == 0) ? slot0_os : slot1_os;

            const board_profile_t *profile = board_profile_get_current();
            uint8_t board_model = profile ? (uint8_t)profile->model : 0;
            const char *board_name = profile ? profile->model_name : "WEMOS 18650";

            const esp_app_desc_t *app_desc = esp_app_get_description();
            char resp[512];
            snprintf(resp, sizeof(resp), 
                     "{\"status\":\"ok\",\"version\":\"%s\",\"btn1_vk\":%d,\"btn1_mod\":%d,\"btn2_vk\":%d,\"btn2_mod\":%d,"
                     "\"btn3_vk\":%d,\"btn3_mod\":%d,\"btn4_vk\":%d,\"btn4_mod\":%d,\"tx_power\":%d,\"sleep_mode\":%d,\"mic_enabled\":%d,"
                     "\"board_model\":%d,\"board_name\":\"%s\",\"sleep_timeout_min\":%d,\"active_slot\":%d,"
                     "\"query_slot\":%d,\"slot0_paired\":%d,\"slot1_paired\":%d,\"slot0_os\":%d,\"slot1_os\":%d,\"query_os\":%d}\n",
                     app_desc->version, vk1, mod1, vk2, mod2, vk3, mod3, vk4, mod4, tx_power, sleep_mode, mic_enabled,
                     board_model, board_name, sleep_timeout_min, active_slot,
                     query_slot, slot0_paired, slot1_paired, slot0_os, slot1_os, query_os);
            respond_cb(resp, strlen(resp));
            
        } else if (strcmp(cmd, "set_host_os") == 0) {
            uint8_t target_slot = multi_host_get_current_slot();
            cJSON *slot_item = cJSON_GetObjectItem(root, "slot");
            if (slot_item && cJSON_IsNumber(slot_item) && slot_item->valueint <= 1) {
                target_slot = slot_item->valueint;
            }
            cJSON *os_item = cJSON_GetObjectItem(root, "os_type");
            if (os_item && cJSON_IsNumber(os_item)) {
                config_storage_save_slot_os_type(target_slot, os_item->valueint);
            }
            const char *resp = "{\"status\":\"ok\"}\n";
            respond_cb(resp, strlen(resp));

        } else if (strcmp(cmd, "set_all_config") == 0) {
            uint8_t target_slot = multi_host_get_current_slot();
            cJSON *slot_item = cJSON_GetObjectItem(root, "slot");
            if (slot_item && cJSON_IsNumber(slot_item) && slot_item->valueint <= 1) {
                target_slot = slot_item->valueint;
            }

            // 0. OS Type (optional)
            cJSON *os_item = cJSON_GetObjectItem(root, "os_type");
            if (os_item && cJSON_IsNumber(os_item)) {
                config_storage_save_slot_os_type(target_slot, os_item->valueint);
            }

            // 1. Buttons
            cJSON *vk_items[4] = {
                cJSON_GetObjectItem(root, "btn1_vk"), cJSON_GetObjectItem(root, "btn2_vk"),
                cJSON_GetObjectItem(root, "btn3_vk"), cJSON_GetObjectItem(root, "btn4_vk")
            };
            cJSON *mod_items[4] = {
                cJSON_GetObjectItem(root, "btn1_mod"), cJSON_GetObjectItem(root, "btn2_mod"),
                cJSON_GetObjectItem(root, "btn3_mod"), cJSON_GetObjectItem(root, "btn4_mod")
            };
            for (int i = 0; i < 4; i++) {
                if (vk_items[i] && mod_items[i] && cJSON_IsNumber(vk_items[i]) && cJSON_IsNumber(mod_items[i])) {
                    config_storage_save_slot_button(target_slot, i, vk_items[i]->valueint, mod_items[i]->valueint);
                }
            }

            // 2. TX Power
            cJSON *tx_item = cJSON_GetObjectItem(root, "tx_power");
            if (tx_item && cJSON_IsNumber(tx_item)) {
                config_storage_save_tx_power(tx_item->valueint);
            }

            // 3. Sleep Mode & Timeout
            cJSON *sleep_item = cJSON_GetObjectItem(root, "sleep_mode");
            if (sleep_item && cJSON_IsNumber(sleep_item)) {
                config_storage_save_slot_sleep_mode(target_slot, sleep_item->valueint);
            }
            cJSON *tmout_item = cJSON_GetObjectItem(root, "sleep_timeout_min");
            if (tmout_item && cJSON_IsNumber(tmout_item)) {
                config_storage_save_slot_sleep_timeout(target_slot, tmout_item->valueint);
            }

            // 4. Mic Enabled
            cJSON *mic_item = cJSON_GetObjectItem(root, "mic_enabled");
            if (mic_item && cJSON_IsNumber(mic_item)) {
                config_storage_save_slot_mic_enabled(target_slot, mic_item->valueint);
            }

            const char *resp = "{\"status\":\"ok\",\"reboot\":1}\n";
            respond_cb(resp, strlen(resp));

            ESP_LOGW(TAG, "Full configuration saved for slot %d. Rebooting in 400ms...", target_slot);
            vTaskDelay(pdMS_TO_TICKS(400));
            esp_restart();

        } else if (strcmp(cmd, "set_btn") == 0) {
            cJSON *idx_item = cJSON_GetObjectItem(root, "idx");
            cJSON *vk_item = cJSON_GetObjectItem(root, "vk");
            cJSON *mod_item = cJSON_GetObjectItem(root, "mod");
            if (idx_item && vk_item && mod_item && cJSON_IsNumber(idx_item) && cJSON_IsNumber(vk_item) && cJSON_IsNumber(mod_item)) {
                uint8_t idx = idx_item->valueint;
                uint8_t vk = vk_item->valueint;
                uint8_t mod = mod_item->valueint;
                config_storage_save_button(idx, vk, mod);
                
                const char *resp = "{\"status\":\"ok\"}\n";
                respond_cb(resp, strlen(resp));
            }
            
        } else if (strcmp(cmd, "set_tx_power") == 0) {
            cJSON *level_item = cJSON_GetObjectItem(root, "level");
            if (level_item && cJSON_IsNumber(level_item)) {
                uint8_t level = level_item->valueint;
                config_storage_save_tx_power(level);
                esp_bredr_tx_power_set((esp_power_level_t)level, (esp_power_level_t)level);
                
                const char *resp = "{\"status\":\"ok\"}\n";
                respond_cb(resp, strlen(resp));
            }
            
        } else if (strcmp(cmd, "set_sleep_mode") == 0) {
            cJSON *enabled_item = cJSON_GetObjectItem(root, "enabled");
            if (enabled_item && cJSON_IsNumber(enabled_item)) {
                uint8_t enabled = enabled_item->valueint;
                config_storage_save_sleep_mode(enabled);
                
                const char *resp = "{\"status\":\"ok\"}\n";
                respond_cb(resp, strlen(resp));
            }
        } else if (strcmp(cmd, "set_mic_enabled") == 0) {
            cJSON *enabled_item = cJSON_GetObjectItem(root, "enabled");
            if (enabled_item && cJSON_IsNumber(enabled_item)) {
                uint8_t enabled = enabled_item->valueint;
                config_storage_save_mic_enabled(enabled);
                
                const char *resp = "{\"status\":\"ok\"}\n";
                respond_cb(resp, strlen(resp));
                
                ESP_LOGI(TAG, "Mic enabled status updated to %d. Resetting system in 1 second...", enabled);
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            }
        } else if (strcmp(cmd, "set_board_model") == 0) {
            cJSON *model_item = cJSON_GetObjectItem(root, "model");
            if (model_item && cJSON_IsNumber(model_item)) {
                uint8_t model = model_item->valueint;
                if (model < BOARD_MODEL_MAX) {
                    board_profile_set_model((board_model_t)model);
                    const char *resp = "{\"status\":\"ok\"}\n";
                    respond_cb(resp, strlen(resp));
                    
                    ESP_LOGI(TAG, "Board model updated to %d. Rebooting in 1 second...", model);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                } else {
                    const char *resp = "{\"status\":\"error\",\"reason\":\"invalid_model\"}\n";
                    respond_cb(resp, strlen(resp));
                }
            }
        } else if (strcmp(cmd, "set_sleep_timeout") == 0) {
            cJSON *min_item = cJSON_GetObjectItem(root, "minutes");
            if (min_item && cJSON_IsNumber(min_item)) {
                uint8_t minutes = (uint8_t)min_item->valueint;
                if (minutes >= 1 && minutes <= 120) {
                    config_storage_save_sleep_timeout(minutes);
                    button_handler_update_sleep_timeout(minutes);
                    const char *resp = "{\"status\":\"ok\"}\n";
                    respond_cb(resp, strlen(resp));
                    ESP_LOGI(TAG, "Sleep timeout updated dynamically to %d min without restart.", minutes);
                } else {
                    const char *resp = "{\"status\":\"error\",\"reason\":\"out_of_range\"}\n";
                    respond_cb(resp, strlen(resp));
                }
            }
        } else if (strcmp(cmd, "switch_host_slot") == 0) {
            cJSON *slot_item = cJSON_GetObjectItem(root, "slot");
            uint8_t current_slot = multi_host_get_current_slot();
            if (slot_item && cJSON_IsNumber(slot_item)) {
                uint8_t target_slot = slot_item->valueint;
                const char *resp = "{\"status\":\"ok\"}\n";
                respond_cb(resp, strlen(resp));
                if (target_slot != current_slot) {
                    ESP_LOGW(TAG, "Host switch requested via SPP: %d -> %d", current_slot, target_slot);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    multi_host_set_slot(target_slot);
                } else {
                    ESP_LOGI(TAG, "Host slot %d already active.", target_slot);
                }
            } else {
                const char *resp = "{\"status\":\"ok\"}\n";
                respond_cb(resp, strlen(resp));
                uint8_t target_slot = (current_slot == 0) ? 1 : 0;
                ESP_LOGW(TAG, "Host switch toggle requested via SPP: %d -> %d", current_slot, target_slot);
                vTaskDelay(pdMS_TO_TICKS(100));
                multi_host_switch_slot();
            }
        } else if (strcmp(cmd, "ota_start") == 0) {
            cJSON *size_item = cJSON_GetObjectItem(root, "size");
            if (size_item && cJSON_IsNumber(size_item)) {
                size_t ota_size = size_item->valueint;
                uint8_t ota_ready = 0;
                config_storage_load_ota_ready(&ota_ready);
                
                if (ota_ready == 1) {
                    esp_err_t err = uart_console_ota_start(ota_size);
                    if (err == ESP_OK) {
                        const char *resp = "{\"status\":\"ok\"}\n";
                        respond_cb(resp, strlen(resp));
                    } else {
                        char resp[128];
                        snprintf(resp, sizeof(resp), "{\"status\":\"error\",\"reason\":\"ota_begin_failed: %d\"}\n", err);
                        respond_cb(resp, strlen(resp));
                    }
                } else {
                    config_storage_save_ota_ready(1);
                    const char *resp = "{\"status\":\"ok\",\"reboot\":1}\n";
                    respond_cb(resp, strlen(resp));
                    
                    ESP_LOGW("CONFIG_CMD", "OTA start requested. Rebooting into clean OTA mode in 500ms...");
                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_restart();
                }
            }
        } else if (strcmp(cmd, "reset_bt_pairing") == 0) {
            uint8_t target_slot = multi_host_get_current_slot();
            cJSON *slot_item = cJSON_GetObjectItem(root, "slot");
            if (slot_item && cJSON_IsNumber(slot_item) && slot_item->valueint <= 1) {
                target_slot = slot_item->valueint;
            }

            // 仅移除指定槽位的配对和 NVS 地址，保护另一台电脑配对
            esp_bd_addr_t saved_addr;
            if (config_storage_load_slot_hfp_addr(target_slot, saved_addr) == ESP_OK) {
                esp_bt_gap_remove_bond_device(saved_addr);
            }
            config_storage_clear_slot_hfp_addr(target_slot);

            const char *resp = "{\"status\":\"ok\"}\n";
            respond_cb(resp, strlen(resp));
            ESP_LOGW(TAG, "Resetting BT pairing for slot %d via SPP. Rebooting in 400ms...", target_slot);
            vTaskDelay(pdMS_TO_TICKS(400));
            esp_restart();
        }
    }
    cJSON_Delete(root);
}
