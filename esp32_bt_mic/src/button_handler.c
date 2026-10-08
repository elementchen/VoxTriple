/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_gap_bt_api.h"
#include "esp_hidd_api.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "button_handler.h"
#include "classic_hidd.h"
#include "config_storage.h"
#include "bt_init.h"
#include "esp_hf_client_api.h"
#include "uart_console.h"
#include "bt_config.h"

static const char *TAG = "BTN_HANDLER";

/* Button GPIO configuration */
#define DEBOUNCE_MS          50
#define LONG_PRESS_MS        1000
#include "esp_bt.h"
#include "esp_bt_main.h"

#define BUTTON_TASK_STACK    4096
#define BUTTON_TASK_PRIORITY 3

#include "board_profile.h"

static gpio_num_t s_indicator_led_gpio = GPIO_NUM_22;
#define DEFAULT_INACTIVITY_MIN   5    /* Default: 5 min deep sleep timeout */
#define MIN_INACTIVITY_MIN       1
#define MAX_INACTIVITY_MIN       120

static TimerHandle_t s_inactivity_timer = NULL;
static gpio_num_t s_button_pins[BUTTON_NUM] = {
    GPIO_NUM_4,
    GPIO_NUM_19,
    GPIO_NUM_23,
    GPIO_NUM_18,
};

static bool is_rtc_gpio(gpio_num_t gpio)
{
    switch (gpio) {
        case GPIO_NUM_0:
        case GPIO_NUM_2:
        case GPIO_NUM_4:
        case GPIO_NUM_12:
        case GPIO_NUM_13:
        case GPIO_NUM_14:
        case GPIO_NUM_15:
        case GPIO_NUM_25:
        case GPIO_NUM_26:
        case GPIO_NUM_27:
        case GPIO_NUM_32:
        case GPIO_NUM_33:
        case GPIO_NUM_34:
        case GPIO_NUM_35:
        case GPIO_NUM_36:
        case GPIO_NUM_37:
        case GPIO_NUM_38:
        case GPIO_NUM_39:
            return true;
        default:
            return false;
    }
}

/* Dedicated task for entering sleep to avoid Tmr Svc task stack overflow */
static void enter_sleep_task(void *pvParameters)
{
    gpio_num_t wakeup_pin = s_button_pins[0];
    bool can_deep_sleep = is_rtc_gpio(wakeup_pin);

    if (can_deep_sleep) {
        ESP_LOGI(TAG, "Inactivity timeout — entering DEEP SLEEP (wakeup pin: GPIO %d)", wakeup_pin);

        /* 1. Fully disable and deinit Bluetooth for maximum power saving in deep sleep */
        #if ENABLE_CLASSIC_BT_MIC
        esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
        esp_hf_client_deinit();
        #endif
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();

        /* 2. Configure RTC wakeup sources */
        rtc_gpio_pullup_en(wakeup_pin);
        rtc_gpio_pulldown_dis(wakeup_pin);
        esp_sleep_enable_ext1_wakeup(1ULL << wakeup_pin, ESP_EXT1_WAKEUP_ALL_LOW);

        vTaskDelay(pdMS_TO_TICKS(100));  /* let log flush */
        esp_deep_sleep_start();
    } else {
        /* If configured pin doesn't support RTC (like GPIO16), fall back to Light Sleep */
        ESP_LOGW(TAG, "Wakeup pin GPIO %d lacks RTC support! Entering LIGHT SLEEP instead...", wakeup_pin);

        /* Disable Bluetooth RF scan to lower Light Sleep power usage */
        esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

        /* Configure GPIO wakeup */
        gpio_wakeup_enable(wakeup_pin, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();

        vTaskDelay(pdMS_TO_TICKS(100));  /* let log flush */
        esp_light_sleep_start();

        /* --- Re-execution continues here upon waking up --- */
        ESP_LOGI(TAG, "Woken up from LIGHT SLEEP! Restoring RF...");

        /* Restore scanner mode */
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        
        /* Clean wakeup configs */
        gpio_intr_disable(wakeup_pin);

        /* Automatically attempt reconnect after light sleep wakeup */
        esp_bd_addr_t saved_addr;
        if (config_storage_load_hfp_addr(saved_addr) == ESP_OK) {
            ESP_LOGI(TAG, "Re-triggering reconnect after wakeup...");
            esp_bt_hid_device_connect(saved_addr);
        }

        /* Reset the idle timer */
        if (s_inactivity_timer) xTimerReset(s_inactivity_timer, 0);
    }
    vTaskDelete(NULL);
}

/* Enter sleep — adaptive selection of Deep Sleep or Light Sleep based on RTC IO capability. */
static void inactivity_sleep_cb(TimerHandle_t xTimer)
{
    xTaskCreate(enter_sleep_task, "SleepTask", 4096, NULL, 5, NULL);
}

static void get_button_mapping(uint8_t button_id, uint8_t *vk_code, uint8_t *modifier)
{
    if (config_storage_load_button(button_id, vk_code, modifier) != ESP_OK) {
        *modifier = 0;
        if (button_id == 0) *vk_code = 0x0D; // Enter
        else if (button_id == 1) *vk_code = 0x1B; // Esc
        else if (button_id == 2) *vk_code = 0x20; // Space
        else if (button_id == 3) *vk_code = 0x09; // Tab
    }
}


typedef enum {
    BTN_STATE_IDLE,
    BTN_STATE_DEBOUNCE,
    BTN_STATE_PRESSED,
} btn_state_t;

static TaskHandle_t s_btn_task_handle = NULL;
static bool s_btn_task_running = false;

void system_reset_bt_pairing(void)
{
    ESP_LOGW(TAG, "Executing BT pairing reset (forget device)...");

    // 1. Flash LED as physical feedback (6 times, 100ms)
    for (int j = 0; j < 6; j++) {
        gpio_set_level(s_indicator_led_gpio, (j % 2 == 0) ? 0 : 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    gpio_set_level(s_indicator_led_gpio, 1);

    // 2. Remove pairing bond info from Bluedroid stack API
    int dev_num = esp_bt_gap_get_bond_device_num();
    if (dev_num > 0) {
        esp_bd_addr_t *dev_list = (esp_bd_addr_t *)malloc(sizeof(esp_bd_addr_t) * dev_num);
        if (dev_list) {
            esp_bt_gap_get_bond_device_list(&dev_num, dev_list);
            for (int i = 0; i < dev_num; i++) {
                esp_bt_gap_remove_bond_device(dev_list[i]);
            }
            free(dev_list);
        }
    }

    esp_bd_addr_t saved_addr;
    if (config_storage_load_hfp_addr(saved_addr) == ESP_OK) {
        esp_bt_gap_remove_bond_device(saved_addr);
    }

    // 3. Physically erase Bluedroid pairing link keys from NVS ("bt_config.conf" namespace)
    nvs_handle_t bt_cfg_handle;
    if (nvs_open("bt_config.conf", NVS_READWRITE, &bt_cfg_handle) == ESP_OK) {
        ESP_LOGI(TAG, "Physically erasing bt_config.conf NVS namespace...");
        nvs_erase_all(bt_cfg_handle);
        nvs_commit(bt_cfg_handle);
        nvs_close(bt_cfg_handle);
    }

    // 4. Clear ONLY the target HFP host address in NVS (preserving key mappings, board profile, sleep timeout, etc.)
    config_storage_clear_hfp_addr();

    // 5. Restart
    ESP_LOGW(TAG, "BT pairings cleared. Restarting now...");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

/**
 * @brief Button monitoring task with debounce
 */
static void button_task_func(void *arg)
{
    btn_state_t state[BUTTON_NUM] = {BTN_STATE_IDLE};
    uint32_t last_change[BUTTON_NUM] = {0};
    uint32_t press_time[BUTTON_NUM] = {0};

    ESP_LOGI(TAG, "Button task started (GPIOs: %d, %d, %d, %d)",
             s_button_pins[0], s_button_pins[1], s_button_pins[2], s_button_pins[3]);

    while (s_btn_task_running) {
        uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

        for (int i = 0; i < BUTTON_NUM; i++) {
            /* Active-low with internal pull-up: 0 = pressed, 1 = released */
            bool pressed = (gpio_get_level(s_button_pins[i]) == 0);

            switch (state[i]) {
            case BTN_STATE_IDLE:
                if (pressed) {
                    state[i] = BTN_STATE_DEBOUNCE;
                    last_change[i] = now;
                }
                break;

            case BTN_STATE_DEBOUNCE:
                if (!pressed) {
                    state[i] = BTN_STATE_IDLE;
                } else if ((now - last_change[i]) >= DEBOUNCE_MS) {
                    state[i] = BTN_STATE_PRESSED;
                    press_time[i] = now;
                    ESP_LOGI(TAG, "Button %d pressed", i + 1);
                    uart_console_send_event_btn(i, 1); // Notify PC over UART
                    if (i == 0) {
                        gpio_set_level(s_indicator_led_gpio, 0); // Turn LED ON (Active Low) when Button 1 is pressed
                    }

                    /* Reset inactivity deep sleep timer */
                    if (s_inactivity_timer) xTimerReset(s_inactivity_timer, 0);

                    /* Connection wake-up & HFP SCO activation */
                    if (classic_hidd_is_connected()) {
                        uint8_t vk = 0, mod = 0;
                        get_button_mapping(i, &vk, &mod);
                        classic_hidd_send_key(mod, vk);
                    } else {
                        // Only Button 1 triggers reconnect
                        if (i == 0) {
                            ESP_LOGW(TAG, "Classic BT HID not connected. Button 1 press triggers reconnect...");
                            esp_bd_addr_t saved_addr;
                            if (config_storage_load_hfp_addr(saved_addr) == ESP_OK) {
                                ESP_LOGI(TAG, "Saved host address found. Initiating reconnect to Host...");
                                esp_bt_hid_device_connect(saved_addr);
                            }
                        }
                    }
                }
                break;

            case BTN_STATE_PRESSED:
                if (!pressed) {
                    uint32_t duration = now - press_time[i];
                    ESP_LOGI(TAG, "Button %d released (duration: %lu ms)", i + 1, duration);
                    uart_console_send_event_btn(i, 0); // Notify PC over UART
                    if (i == 0) {
                        gpio_set_level(s_indicator_led_gpio, 1); // Turn LED OFF (Active Low) when Button 1 is released
                    }
                    if (classic_hidd_is_connected()) {
                        classic_hidd_release_key();
                    }

                    state[i] = BTN_STATE_IDLE;
                }
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));  /* 10ms polling */
    }

    ESP_LOGI(TAG, "Button task stopped");
    vTaskDelete(NULL);
}

void button_handler_init(void)
{
    ESP_LOGI(TAG, "Initializing button handler");

    const board_profile_t *profile = board_profile_get_current();
    if (profile) {
        s_indicator_led_gpio = profile->led_gpio;
        for (int i = 0; i < BUTTON_NUM; i++) {
            s_button_pins[i] = profile->btn_gpios[i];
        }
        ESP_LOGI(TAG, "Applied board profile: %s (LED=%d)", profile->model_name, s_indicator_led_gpio);
    }

    /* Configure GPIO pins as input with pull-up */
    gpio_config_t io_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    for (int i = 0; i < BUTTON_NUM; i++) {
        io_conf.pin_bit_mask |= (1ULL << s_button_pins[i]);
    }

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Indicator LED — simple GPIO, no RMT/DMA conflict with BT */
    gpio_set_direction(s_indicator_led_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(s_indicator_led_gpio, 1); // Default to OFF (1)

    /* Start button monitoring task */
    s_btn_task_running = true;
    xTaskCreate(button_task_func, "BtnTask", BUTTON_TASK_STACK,
                NULL, BUTTON_TASK_PRIORITY, &s_btn_task_handle);

    /* Start inactivity deep sleep timer — load timeout from NVS */
    uint8_t timeout_min = DEFAULT_INACTIVITY_MIN;
    if (config_storage_load_sleep_timeout(&timeout_min) != ESP_OK) {
        ESP_LOGI(TAG, "No saved sleep timeout, using default %d min", DEFAULT_INACTIVITY_MIN);
        config_storage_save_sleep_timeout(DEFAULT_INACTIVITY_MIN);
        timeout_min = DEFAULT_INACTIVITY_MIN;
    }
    if (timeout_min < MIN_INACTIVITY_MIN || timeout_min > MAX_INACTIVITY_MIN) {
        timeout_min = DEFAULT_INACTIVITY_MIN;
    }
    uint32_t inactivity_ms = (uint32_t)timeout_min * 60 * 1000;
    ESP_LOGI(TAG, "Inactivity sleep timeout: %d min (%lu ms)", timeout_min, (unsigned long)inactivity_ms);
    s_inactivity_timer = xTimerCreate("inact_tmr",
                                       pdMS_TO_TICKS(inactivity_ms),
                                       pdFALSE, NULL, inactivity_sleep_cb);
    if (s_inactivity_timer) xTimerStart(s_inactivity_timer, 0);

    /* Flash LED 3 times quickly as visual wakeup feedback on boot (Active Low) */
    for (int i = 0; i < 6; i++) {
        gpio_set_level(s_indicator_led_gpio, (i % 2 == 0) ? 0 : 1); // 0: ON, 1: OFF
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    gpio_set_level(s_indicator_led_gpio, 1); // Keep it OFF (1)

    ESP_LOGI(TAG, "Button handler initialized");
}

void button_handler_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing button handler");

    s_btn_task_running = false;

    if (s_btn_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(50));
        s_btn_task_handle = NULL;
    }
}
