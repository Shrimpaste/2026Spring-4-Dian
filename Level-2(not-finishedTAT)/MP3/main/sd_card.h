/**
 * @file sd_card.h
 * @brief SD Card interface for MP3 player
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#include "mp3_player.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SD_MOUNT_POINT      "/sdcard"
#define SD_MAX_OPEN_FILES   5
#define SD_SPI_FREQ_KHZ     400  // 400KHz for SD card initialization (tested working)

/**
 * @brief Initialize SD card using SPI interface
 *
 * This function initializes the SD card with the correct SPI pin configuration
 * for ESP32-S3, avoiding conflicts with USB pins (GPIO19/20) and I2S pins.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_card_init(void);

/**
 * @brief Deinitialize SD card
 * @return ESP_OK on success
 */
esp_err_t sd_card_deinit(void);

/**
 * @brief Check if SD card is mounted
 * @return true if mounted, false otherwise
 */
bool sd_card_is_mounted(void);

/**
 * @brief Get SD card information
 * @param[out] out_info Pointer to store card info
 * @return ESP_OK on success
 */
esp_err_t sd_card_get_info(sdmmc_card_t **out_info);

/**
 * @brief Print SD card information
 */
void sd_card_print_info(void);

/**
 * @brief Scan directory for MP3 files
 * @param[in] path Directory path to scan
 * @param[out] files Array to store file names
 * @param[in] max_files Maximum number of files to store
 * @return Number of MP3 files found
 */
int sd_card_scan_mp3_files(const char *path, char **files, int max_files);

/**
 * @brief Get file size
 * @param[in] filepath Full file path
 * @return File size in bytes, or -1 on error
 */
long sd_card_get_file_size(const char *filepath);

#ifdef __cplusplus
}
#endif

#endif /* SD_CARD_H */
