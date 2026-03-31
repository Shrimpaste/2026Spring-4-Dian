idf_component_register(
    SRCS "mp3_player.c" "i2s_driver.c" "main.c"
    INCLUDE_DIRS "." "include"
    PRIV_REQUIRES driver esp_timer freertos spiffs esp_driver_i2s esp_driver_gpio esp_driver_uart minimp3
)

# Define MINIMP3_IMPLEMENTATION to include the implementation
target_compile_definitions(${COMPONENT_LIB} PRIVATE MINIMP3_IMPLEMENTATION)

# Create a SPIFFS image from the flash/data directory
# This will pack all files in flash/data into the SPIFFS partition
spiffs_create_partition_image(storage ${CMAKE_CURRENT_SOURCE_DIR}/../flash/data FLASH_IN_PROJECT)
