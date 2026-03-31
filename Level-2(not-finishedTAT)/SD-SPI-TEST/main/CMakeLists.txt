idf_component_register(SRCS "sd_card_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES fatfs sdmmc
                       PRIV_REQUIRES spi_flash)
