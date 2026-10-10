idf_component_register(SRCS "main.c"
                            "bt_init.c"
                            "bt_hfp_hf.c"
                            "bt_app_core.c"
                            "bt_app_hf.c"
                            "audio_capture.c"
                            "button_handler.c"
                            "config_storage.c"
                            "ws2812_led.c"
                            "classic_hidd.c"
                            "config_cmd.c"
                            "uart_console.c"
                            "board_profile.c"
                            "multi_host.c"
                    PRIV_REQUIRES bt nvs_flash driver esp_ringbuf led_strip
                                 esp_hid json app_update
                    INCLUDE_DIRS "."
                    WHOLE_ARCHIVE)
