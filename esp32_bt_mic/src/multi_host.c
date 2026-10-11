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
#include "uart_console.h"

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

static TaskHandle_t s_switch_monitor_task = NULL;

void multi_host_trigger_led_feedback(uint8_t slot)
{
    xTaskCreate(led_feedback_task, "led_fb_task", 2048, (void *)(uint32_t)slot, 4, NULL);
}

uint8_t multi_host_read_hardware_switch_slot(void)
{
    // GPIO 33 内部弱上拉使能：
    // 电平 1 (未接地/悬空) = Slot 0 (Pair 1)
    // 电平 0 (接 GND)    = Slot 1 (Pair 2)
    int level = gpio_get_level(MULTI_HOST_SWITCH_GPIO);
    return (level == 0) ? 1 : 0;
}

static void hardware_switch_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Hardware switch monitor task started (GPIO %d)", MULTI_HOST_SWITCH_GPIO);
    uint8_t last_sampled_slot = multi_host_read_hardware_switch_slot();
    uint8_t debounce_count = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms 轮询检测
        uint8_t current_slot = multi_host_read_hardware_switch_slot();

        if (current_slot != last_sampled_slot) {
            debounce_count++;
            if (debounce_count >= 3) { // 连续 150ms 稳定保持新状态，防抖通过
                ESP_LOGW(TAG, "Hardware slide switch toggled detected: Slot %d -> %d", last_sampled_slot, current_slot);
                last_sampled_slot = current_slot;
                debounce_count = 0;

                // 若物理档位与当前正在运行的活跃槽位不一致，触发物理切换
                if (current_slot != s_active_slot) {
                    multi_host_set_slot(current_slot);
                }
            }
        } else {
            debounce_count = 0;
        }
    }
}

esp_err_t multi_host_init(void)
{
    // 1. 初始化 GPIO 33 两档拨动开关引脚（输入模式 + 内部弱上拉）
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MULTI_HOST_SWITCH_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure switch GPIO %d: %s", MULTI_HOST_SWITCH_GPIO, esp_err_to_name(ret));
    }

    // 等待 15ms 让内部弱上拉电平充分建立
    vTaskDelay(pdMS_TO_TICKS(15));

    // 2. 读取物理拨动开关档位
    uint8_t hw_slot = multi_host_read_hardware_switch_slot();
    config_storage_get_active_slot(&s_active_slot);

    ESP_LOGI(TAG, "Hardware Slide Switch: GPIO %d level=%d (Mapped Slot: %d), NVS Slot: %d",
             MULTI_HOST_SWITCH_GPIO, gpio_get_level(MULTI_HOST_SWITCH_GPIO), hw_slot, s_active_slot);

    // 3. 物理开关档位拥有最高权威性：开机若与 NVS 不符，则以硬件物理开关为准更新 NVS
    if (hw_slot != s_active_slot) {
        ESP_LOGW(TAG, "Syncing active slot with hardware switch: NVS %d -> Hardware %d", s_active_slot, hw_slot);
        s_active_slot = hw_slot;
        config_storage_set_active_slot(hw_slot);
    }

    // 4. 开机根据当前活跃槽位触发物理 LED 闪烁反馈 (槽位0: 单闪3次; 槽位1: 双闪3次)
    multi_host_trigger_led_feedback(s_active_slot);

    // 5. 启动运行时物理拨动开关消抖监控任务
    if (s_switch_monitor_task == NULL) {
        xTaskCreate(hardware_switch_monitor_task, "hw_sw_task", 2048, NULL, 5, &s_switch_monitor_task);
    }

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
    ESP_LOGW(TAG, ">>> SWITCHING HOST: Slot %d -> Slot %d (Restarting for clean baseband reset) <<<", prev_slot, new_slot);
    ESP_LOGW(TAG, "========================================");

    // 1. 保存目标槽位至 NVS
    s_active_slot = new_slot;
    config_storage_set_active_slot(new_slot);

    // 2. 主动向 USB 串口上位机发送槽位热切换通知
    uart_console_send_event_slot(new_slot);

    // 3. 触发 LED 闪烁反馈
    multi_host_trigger_led_feedback(new_slot);

    // 3. 延时 400ms 保证客户端回执和日志写完，随后整机干净重启重置 Bluedroid 协议栈！
    vTaskDelay(pdMS_TO_TICKS(400));
    esp_restart();

    return ESP_OK;
}

esp_err_t multi_host_switch_slot(void)
{
    return multi_host_set_slot((s_active_slot == 0) ? 1 : 0);
}


