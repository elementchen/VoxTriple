#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_gap_bt_api.h"
#include "esp_hidd_api.h"
#include "esp_hf_client_api.h"
#include "driver/gpio.h"
#include "config_storage.h"
#include "board_profile.h"
#include "classic_hidd.h"
#include "bt_init.h"
#include "multi_host.h"

static const char *TAG = "MULTI_HOST";
static uint8_t s_active_slot = 0;

static void led_feedback_task(void *pvParameters)
{
    uint32_t slot = (uint32_t)pvParameters;
    gpio_num_t led_gpio = board_profile_get_current()->led_gpio;

    // Active Low LED: 0 = ON, 1 = OFF
    if (slot == 0) {
        // Slot 0 (设备一): 闪动一下，连续3次 (亮150ms -> 灭250ms)
        ESP_LOGI(TAG, "LED Feedback: Slot 0 (Single flash x 3)");
        for (int i = 0; i < 3; i++) {
            gpio_set_level(led_gpio, 0); // 亮
            vTaskDelay(pdMS_TO_TICKS(150));
            gpio_set_level(led_gpio, 1); // 灭
            vTaskDelay(pdMS_TO_TICKS(250));
        }
    } else {
        // Slot 1 (设备二): 双闪两下，连续3次 ([亮100ms -> 灭100ms -> 亮100ms -> 灭350ms] x 3)
        ESP_LOGI(TAG, "LED Feedback: Slot 1 (Double flash x 3)");
        for (int i = 0; i < 3; i++) {
            gpio_set_level(led_gpio, 0); // 亮 1
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(led_gpio, 1); // 灭 1
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(led_gpio, 0); // 亮 2
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(led_gpio, 1); // 灭 2
            vTaskDelay(pdMS_TO_TICKS(350));
        }
    }
    gpio_set_level(led_gpio, 1); // 确保恢复熄灭
    vTaskDelete(NULL);
}

void multi_host_trigger_led_feedback(uint8_t slot)
{
    xTaskCreate(led_feedback_task, "led_fb_task", 2048, (void *)(uint32_t)slot, 4, NULL);
}

esp_err_t multi_host_init(void)
{
    config_storage_get_active_slot(&s_active_slot);
    ESP_LOGI(TAG, "Multi-host initialized. Current Active Slot: %d", s_active_slot);
    return ESP_OK;
}

uint8_t multi_host_get_current_slot(void)
{
    return s_active_slot;
}

esp_err_t multi_host_set_slot(uint8_t new_slot)
{
    if (new_slot > 1) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t prev_slot = s_active_slot;
    if (new_slot == prev_slot) {
        ESP_LOGI(TAG, "Slot %d is already active. Retriggering LED feedback.", new_slot);
        multi_host_trigger_led_feedback(new_slot);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, ">>> SWITCHING HOST: Slot %d -> Slot %d <<<", prev_slot, new_slot);
    ESP_LOGW(TAG, "========================================");

    // 1. 优雅断开当前主机的连接
    if (classic_hidd_is_connected()) {
        ESP_LOGI(TAG, "Disconnecting current HID link...");
        esp_bt_hid_device_disconnect();
    }

    if (bt_hfp_is_connected()) {
        esp_bd_addr_t prev_addr;
        if (config_storage_load_slot_hfp_addr(prev_slot, prev_addr) == ESP_OK) {
            ESP_LOGI(TAG, "Disconnecting current HFP link...");
            esp_hf_client_disconnect(prev_addr);
        }
    }

    // 短暂避让 80ms 确保断开报文安全发出
    vTaskDelay(pdMS_TO_TICKS(80));

    // 2. 更新活跃槽位至 NVS
    s_active_slot = new_slot;
    config_storage_set_active_slot(new_slot);

    // 3. 触发 LED 闪烁反馈动画
    multi_host_trigger_led_feedback(new_slot);

    // 4. 主动重连目标槽位设备或开启广播等待配对
    esp_bd_addr_t target_addr = {0};
    if (config_storage_load_slot_hfp_addr(new_slot, target_addr) == ESP_OK) {
        ESP_LOGI(TAG, "Saved target address found for Slot %d: %02x:%02x:%02x:%02x:%02x:%02x",
                 new_slot, target_addr[0], target_addr[1], target_addr[2],
                 target_addr[3], target_addr[4], target_addr[5]);
        ESP_LOGI(TAG, "Initiating active reconnect to target host...");
        esp_bt_hid_device_connect(target_addr);
        esp_hf_client_connect(target_addr);
    } else {
        ESP_LOGW(TAG, "No saved host address in Slot %d. Enabling discovery mode for initial pairing...", new_slot);
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    }

    return ESP_OK;
}

esp_err_t multi_host_switch_slot(void)
{
    return multi_host_set_slot((s_active_slot == 0) ? 1 : 0);
}

