idf_component_register(SRCS "config_manager.c" "main.c" "mp3_player.c" "sd_card.c" "i2s_audio.c" "console_cmd.c"
                       INCLUDE_DIRS "."
                       REQUIRES fatfs nvs_flash driver esp_adc esp_timer esp_driver_i2s esp_driver_spi esp_driver_uart sdmmc)
