idf_component_register(SRCS "iperf_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi nvs_flash iperf)