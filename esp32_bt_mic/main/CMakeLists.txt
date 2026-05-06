idf_component_register(SRCS "main.c"
                            "bt_init.c"
                            "bt_hfp_ag.c"
                            "bt_app_core.c"
                            "bt_app_hf.c"
                            "ble_gatts_config.c"
                            "audio_capture.c"
                            "audio_task.c"
                            "button_handler.c"
                            "config_storage.c"
                    PRIV_REQUIRES bt nvs_flash driver esp_ringbuf
                    INCLUDE_DIRS ".")
