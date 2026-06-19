idf_component_register(SRCS "main.c"
                            "bt_init.c"
                            "nimble_gap.c"
                            "button_handler.c"
                            "config_storage.c"
                            "ws2812_led.c"
                            "ble_hid_keyboard.c"
                            "esp_hid_gap.c"
                    PRIV_REQUIRES bt nvs_flash driver esp_ringbuf led_strip
                                 esp_hid
                    INCLUDE_DIRS ".")
