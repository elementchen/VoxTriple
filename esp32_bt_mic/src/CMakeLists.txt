idf_component_register(SRCS "main.c"
                            "bt_init.c"
                            "bt_hfp_hf.c"
                            "bt_app_core.c"
                            "bt_app_hf.c"
                            "ble_gatts_config.c"
                            "audio_capture.c"
                            "button_handler.c"
                            "config_storage.c"
                            "ws2812_led.c"
                            "ble_hid_keyboard.c"
                    PRIV_REQUIRES bt nvs_flash driver esp_ringbuf led_strip
                                 esp_hid
                    INCLUDE_DIRS ".")
