/*
 * SPDX-FileCopyrightText: 2024 ESP32 BT Microphone Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * USB Serial OTA firmware receiver. Monitors UART0 for "OTA:START" command.
 * After handshake, switches to 921600 bps for fast binary transfer,
 * then reverts to 115200 for completion handshake.
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "uart_ota.h"

static const char *TAG = "UART_OTA";

#define OTA_UART_NUM       UART_NUM_0
#define OTA_BAUD_NORMAL    115200
#define OTA_BAUD_FAST      921600
#define OTA_CHUNK          4096
#define OTA_STACK_SIZE     6144
#define OTA_TASK_PRIO      2

static void uart_ota_task(void *arg)
{
    uint8_t *buf = NULL;
    uint32_t total = 0, received = 0;
    char line[64];
    int lp = 0;

    buf = (uint8_t *)malloc(OTA_CHUNK);
    if (!buf) { vTaskDelete(NULL); return; }

    while (1) {
        uint8_t ch;
        int n = uart_read_bytes(OTA_UART_NUM, &ch, 1, pdMS_TO_TICKS(200));
        if (n <= 0) continue;

        if (ch == '\n') {
            line[lp] = '\0';
            lp = 0;

            if (strncmp(line, "OTA:START:", 10) == 0) {
                total = strtoul(line + 10, NULL, 10);
                if (total == 0 || total > 4*1024*1024) {
                    uart_write_bytes(OTA_UART_NUM, "OTA:ERR:SIZE\n", 13);
                    continue;
                }

                /* Begin OTA */
                const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
                esp_ota_handle_t handle;
                if (!part || esp_ota_begin(part, total, &handle) != ESP_OK) {
                    uart_write_bytes(OTA_UART_NUM, "OTA:ERR:BEGIN\n", 14);
                    continue;
                }

                uart_write_bytes(OTA_UART_NUM, "OTA:OK\n", 7);
                ESP_LOGI(TAG, "OTA: %" PRIu32 " bytes @ %d bps", total, OTA_BAUD_FAST);

                /* Wait for host to ack, then switch to fast baud */
                vTaskDelay(pdMS_TO_TICKS(200));
                uart_flush_input(OTA_UART_NUM);
                uart_set_baudrate(OTA_UART_NUM, OTA_BAUD_FAST);
                esp_log_level_set("*", ESP_LOG_NONE);  /* suppress log to UART */

                /* Receive binary stream at fast baud */
                received = 0;
                while (received < total) {
                    uint32_t want = total - received;
                    if (want > OTA_CHUNK) want = OTA_CHUNK;
                    n = uart_read_bytes(OTA_UART_NUM, buf, want, pdMS_TO_TICKS(10000));
                    if (n > 0) {
                        if (esp_ota_write(handle, buf, n) != ESP_OK) {
                            esp_ota_end(handle);
                            goto ota_fail;
                        }
                        received += n;
                    }
                }

                /* Back to normal baud, wait for END confirmation */
                uart_set_baudrate(OTA_UART_NUM, OTA_BAUD_NORMAL);
                esp_log_level_set("*", ESP_LOG_WARN);
                vTaskDelay(pdMS_TO_TICKS(100));
                uart_flush_input(OTA_UART_NUM);

                /* Read "OTA:END\n" line at normal baud */
                int end_lp = 0;
                char end_line[16] = {0};
                while (end_lp < (int)sizeof(end_line)-1) {
                    if (uart_read_bytes(OTA_UART_NUM, &ch, 1, pdMS_TO_TICKS(5000)) <= 0)
                        break;
                    if (ch == '\n') { end_line[end_lp] = '\0'; break; }
                    end_line[end_lp++] = (char)ch;
                }

                if (strcmp(end_line, "OTA:END") == 0) {
                    esp_ota_end(handle);
                    esp_ota_set_boot_partition(part);
                    uart_write_bytes(OTA_UART_NUM, "OTA:DONE\n", 9);
                    ESP_LOGI(TAG, "OTA: done, rebooting");
                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_restart();
                } else {
                    esp_ota_end(handle);
                    uart_write_bytes(OTA_UART_NUM, "OTA:ERR:END\n", 12);
                }
                continue;

            ota_fail:
                uart_set_baudrate(OTA_UART_NUM, OTA_BAUD_NORMAL);
                esp_log_level_set("*", ESP_LOG_WARN);
                uart_write_bytes(OTA_UART_NUM, "OTA:ERR:WRITE\n", 14);
            }
        } else if (lp < (int)sizeof(line)-1) {
            line[lp++] = (char)ch;
        }
    }
}

void uart_ota_init(void)
{
    uart_config_t cfg = {
        .baud_rate = OTA_BAUD_NORMAL,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(OTA_UART_NUM, &cfg);
    uart_set_pin(OTA_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(uart_ota_task, "uart_ota", OTA_STACK_SIZE,
                NULL, OTA_TASK_PRIO, NULL);
    ESP_LOGI(TAG, "Serial OTA listener started");
}
