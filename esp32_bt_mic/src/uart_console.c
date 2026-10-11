#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "config_cmd.h"
#include "uart_console.h"
#include "audio_capture.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "config_storage.h"

#define TAG "UART_CONSOLE"
#define RX_BUF_SIZE     1024

typedef enum {
    UART_RX_MODE_JSON,
    UART_RX_MODE_BINARY
} uart_rx_mode_t;

static uart_rx_mode_t s_rx_mode = UART_RX_MODE_JSON;
static size_t s_ota_total_size = 0;
static size_t s_ota_received_size = 0;
static esp_ota_handle_t s_ota_handle = 0;
static const esp_partition_t *s_update_partition = NULL;

static char s_uart_rx_buf[RX_BUF_SIZE];
static size_t s_uart_rx_len = 0;

static void uart_respond_cb(const char *data, size_t len)
{
    // Write directly to stdout (fd = 1)
    write(1, data, len);
}

static void handle_uart_line(const char *line, size_t len)
{
    if (len > 0 && line[0] == '{') {
        execute_config_cmd(line, len, uart_respond_cb);
    }
}

static void handle_uart_data(const uint8_t *data, size_t len)
{
    if (s_uart_rx_len + len >= RX_BUF_SIZE) {
        ESP_LOGW(TAG, "UART RX buffer overflow, clearing");
        s_uart_rx_len = 0;
    }
    
    memcpy(s_uart_rx_buf + s_uart_rx_len, data, len);
    s_uart_rx_len += len;
    s_uart_rx_buf[s_uart_rx_len] = '\0';
    
    char *line_start = s_uart_rx_buf;
    char *line_end;
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';
        char *r_char = strchr(line_start, '\r');
        if (r_char) *r_char = '\0';
        
        if (strlen(line_start) > 0) {
            handle_uart_line(line_start, strlen(line_start));
        }
        
        line_start = line_end + 1;
    }
    
    size_t processed_len = line_start - s_uart_rx_buf;
    if (processed_len > 0) {
        if (processed_len < s_uart_rx_len) {
            memmove(s_uart_rx_buf, line_start, s_uart_rx_len - processed_len);
            s_uart_rx_len -= processed_len;
        } else {
            s_uart_rx_len = 0;
        }
    }
}

static void handle_ota_binary_data(const uint8_t *data, size_t len)
{
    esp_err_t err = esp_ota_write(s_ota_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
        
        // Reset state on failure
        s_rx_mode = UART_RX_MODE_JSON;
        esp_ota_end(s_ota_handle);
        
        // Clear NVS ota_ready flag so it boots normally on next power cycle
        config_storage_save_ota_ready(0);
        
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"status\":\"error\",\"reason\":\"write_failed: %s\"}\n", esp_err_to_name(err));
        write(1, resp, strlen(resp));
        return;
    }
    
    size_t prev_received = s_ota_received_size;
    s_ota_received_size += len;
    
    // Throttled acknowledgment scheme based on absolute byte counts to ensure 100% phase-shift protection
    bool cross_boundary = (s_ota_received_size / 1024) > (prev_received / 1024);
    bool is_last = (s_ota_received_size >= s_ota_total_size);
    if (cross_boundary || is_last) {
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"status\":\"next\",\"written\":%d,\"total_written\":%d}\n", (int)(s_ota_received_size - prev_received), (int)s_ota_received_size);
        write(1, resp, strlen(resp));
    }
    
    // Verify if download completed
    if (s_ota_received_size >= s_ota_total_size) {
        ESP_LOGI(TAG, "All OTA bytes received (%d/%d). Finalizing...", (int)s_ota_received_size, (int)s_ota_total_size);
        
        err = esp_ota_end(s_ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
            s_rx_mode = UART_RX_MODE_JSON;
            char err_resp[128];
            snprintf(err_resp, sizeof(err_resp), "{\"status\":\"error\",\"reason\":\"ota_end_failed: %s\"}\n", esp_err_to_name(err));
            write(1, err_resp, strlen(err_resp));
            return;
        }
        
        err = esp_ota_set_boot_partition(s_update_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
            s_rx_mode = UART_RX_MODE_JSON;
            const char *err_resp = "{\"status\":\"error\",\"reason\":\"set_boot_failed\"}\n";
            write(1, err_resp, strlen(err_resp));
            return;
        }
        
        // Clear NVS ota_ready flag so it boots normally next time
        config_storage_save_ota_ready(0);
        
        s_rx_mode = UART_RX_MODE_JSON;
        const char *done_resp = "{\"status\":\"done\"}\n";
        write(1, done_resp, strlen(done_resp));
        
        ESP_LOGI(TAG, "OTA Success! Restarting in 1.5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(1500));
        esp_restart();
    }
}

static void uart_rx_task(void *pvParameters)
{
    // Match the 1024-byte chunk size to capture complete packets in a single read call
    uint8_t *data = (uint8_t *) malloc(1024);
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for UART RX task");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UART RX Task started using direct hardware ringbuffer");
    while (1) {
        // Direct read bypasses VFS filters, giving us 100% raw binary stream (no Ctrl+C/Ctrl+D filtering)
        int len = uart_read_bytes(0, data, 1024, pdMS_TO_TICKS(50));
        if (len > 0) {
            if (s_rx_mode == UART_RX_MODE_JSON) {
                handle_uart_data(data, len);
            } else {
                handle_ota_binary_data(data, len);
            }
        }
    }
    free(data);
    vTaskDelete(NULL);
}

esp_err_t uart_console_init(void)
{
    // 1. Configure the physical UART 0 attributes
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(0, &uart_config);

    // 2. Install the hardware UART driver with a large 2048-byte RingBuffer
    uart_driver_install(0, 2048, 0, 0, NULL, 0);

    // 3. Bind VFS stdout/stderr descriptors so log printing still routes correctly
    esp_vfs_dev_uart_use_driver(0);

    // 4. Launch reader task
    xTaskCreate(uart_rx_task, "uart_rx_task", 3072, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t uart_console_ota_start(size_t size)
{
    s_update_partition = esp_ota_get_next_update_partition(NULL);
    if (!s_update_partition) {
        ESP_LOGE(TAG, "No OTA partition found!");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGW(TAG, "Suspending audio and Bluetooth services to prevent DMA/Interrupt conflicts during flash erase...");
    
    // 1. Deinit audio capture and disable I2S DMA interrupts to prevent cache error during flash writing
    audio_capture_stop();
    audio_capture_deinit();
    
    // 2. Disable Bluetooth RF to prevent radio interrupts and NVS flash contention
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    
    // Clear underlying hardware RX RingBuffer immediately to purge any console bytes
    uart_flush_input(0);
    
    esp_err_t err = esp_ota_begin(s_update_partition, size, &s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return err;
    }
    
    s_ota_total_size = size;
    s_ota_received_size = 0;
    s_rx_mode = UART_RX_MODE_BINARY;
    
    ESP_LOGI(TAG, "OTA Mode activated. Size=%d bytes.", (int)size);
    return ESP_OK;
}

void uart_console_send_event_btn(uint8_t btn_id, uint8_t state)
{
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"event\":\"btn\",\"id\":%d,\"state\":%d}\n", btn_id, state);
    write(1, resp, strlen(resp));
}

void uart_console_send_event_status(uint8_t hfp_connected, uint8_t audio_active)
{
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"event\":\"status\",\"hfp\":%d,\"audio\":%d}\n", hfp_connected, audio_active);
    write(1, resp, strlen(resp));
}

void uart_console_send_event_slot(uint8_t slot)
{
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"event\":\"slot\",\"active_slot\":%d}\n", slot);
    write(1, resp, strlen(resp));
}
