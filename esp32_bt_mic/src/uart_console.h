#ifndef UART_CONSOLE_H
#define UART_CONSOLE_H

#include "esp_err.h"

esp_err_t uart_console_init(void);
esp_err_t uart_console_ota_start(size_t size);
void uart_console_send_event_btn(uint8_t btn_id, uint8_t state);
void uart_console_send_event_status(uint8_t hfp_connected, uint8_t audio_active);
void uart_console_send_event_slot(uint8_t slot);

#endif // UART_CONSOLE_H
