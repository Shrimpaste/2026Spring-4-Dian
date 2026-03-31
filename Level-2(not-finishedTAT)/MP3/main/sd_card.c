/**
 * @file sd_card.c
 * @brief SD Card implementation with SPI interface
 *
 * Pin mapping for ESP32-S3 (tested working configuration):
 * - GPIO4:  MOSI (SD_CMD/DI)
 * - GPIO2:  SCK (SCLK/CLK)
 * - GPIO5:  MISO (SD_DO/DO)
 * - GPIO8:  CS (SS)
 *
 * Important: GPIO19 and GPIO20 are reserved for USB on ESP32-S3
 */

#include "sd_card.h"
#include "sdkconfig.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_master.h"
#include <dirent.h>
#include <sys/stat.h>

static const char *TAG = "SD_CARD";

static sdmmc_card_t *s_card = NULL;
static spi_host_device_t s_spi_host_slot = SPI2_HOST;
static bool s_mounted = false;

esp_err_t sd_card_init(void)
{
    esp_err_t ret;

    if (s_mounted) {
        ESP_LOGW(TAG, "SD card already mounted");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SD card using SPI...");
    ESP_LOGI(TAG, "MOSI: GPIO%d, MISO: GPIO%d, SCK: GPIO%d, CS: GPIO%d",
             CONFIG_MP3_SD_MOSI_GPIO, CONFIG_MP3_SD_MISO_GPIO,
             CONFIG_MP3_SD_SCK_GPIO, CONFIG_MP3_SD_CS_GPIO);

    /* IMPORTANT: Initialize host as LOCAL variable (not global static)
     * This ensures SDSPI_HOST_DEFAULT() macro is evaluated at runtime,
     * which is necessary for proper function pointer initialization in ESP-IDF v6.1
     */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    /* Select SPI host based on configuration */
    #if CONFIG_MP3_SPI_HOST_SPI1
    host.slot = SPI1_HOST;
    #elif CONFIG_MP3_SPI_HOST_SPI3
    host.slot = SPI3_HOST;
    #else
    host.slot = SPI2_HOST;  /* Default: HSPI */
    #endif
    s_spi_host_slot = host.slot;

    /* Set SPI frequency for stability */
    host.max_freq_khz = SD_SPI_FREQ_KHZ;
    ESP_LOGI(TAG, "SPI frequency set to %d kHz", SD_SPI_FREQ_KHZ);

    /* Configure SPI bus */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_MP3_SD_MOSI_GPIO,
        .miso_io_num = CONFIG_MP3_SD_MISO_GPIO,
        .sclk_io_num = CONFIG_MP3_SD_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPI bus initialized successfully");

    /* Configure SD card slot */
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CONFIG_MP3_SD_CS_GPIO;
    slot_config.host_id = host.slot;

    /* Mount configuration */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = SD_MAX_OPEN_FILES,
        .allocation_unit_size = 16 * 1024
    };

    /* Mount the filesystem */
    ESP_LOGI(TAG, "Mounting SD card...");
    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config,
                                   &mount_config, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        spi_bus_free(host.slot);
        return ret;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "SD card mounted successfully at %s", SD_MOUNT_POINT);

    /* Print card info */
    sd_card_print_info();

    return ESP_OK;
}

esp_err_t sd_card_deinit(void)
{
    if (!s_mounted) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Unmounting SD card...");

    /* Unmount filesystem */
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount: %s", esp_err_to_name(ret));
    }

    /* Free SPI bus */
    ret = spi_bus_free(s_spi_host_slot);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to free SPI bus: %s", esp_err_to_name(ret));
    }

    s_card = NULL;
    s_mounted = false;

    ESP_LOGI(TAG, "SD card unmounted");
    return ESP_OK;
}

bool sd_card_is_mounted(void)
{
    return s_mounted;
}

esp_err_t sd_card_get_info(sdmmc_card_t **out_info)
{
    if (!s_mounted || s_card == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    *out_info = s_card;
    return ESP_OK;
}

void sd_card_print_info(void)
{
    if (!s_mounted || s_card == NULL) {
        ESP_LOGE(TAG, "SD card not mounted");
        return;
    }

    ESP_LOGI(TAG, "===== SD Card Info =====");
    ESP_LOGI(TAG, "Name: %s", s_card->cid.name);
    ESP_LOGI(TAG, "Type: %s", (s_card->ocr & (1 << 30)) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI(TAG, "Speed: %s", (s_card->csd.tr_speed > 25000000) ? "High Speed" : "Default Speed");

    uint64_t capacity_bytes = ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size;
    ESP_LOGI(TAG, "Capacity: %llu MB (%llu GB)",
             capacity_bytes / (1024 * 1024),
             capacity_bytes / (1024 * 1024 * 1024));

    ESP_LOGI(TAG, "Sector size: %d", s_card->csd.sector_size);
    ESP_LOGI(TAG, "SPI max frequency: %d MHz", s_card->max_freq_khz / 1000);
    ESP_LOGI(TAG, "=======================");
}

int sd_card_scan_mp3_files(const char *path, char **files, int max_files)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return 0;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return 0;
    }

    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL && count < max_files) {
        /* Skip hidden files and directories */
        if (entry->d_name[0] == '.') {
            continue;
        }

        /* Check if it's a file */
        if (entry->d_type == DT_REG) {
            /* Check if it's an MP3 file (case insensitive) */
            size_t len = strlen(entry->d_name);
            if (len > 4) {
                const char *ext = entry->d_name + len - 4;
                if (strcasecmp(ext, ".mp3") == 0) {
                    strncpy(files[count], entry->d_name, MAX_FILE_NAME_LEN - 1);
                    files[count][MAX_FILE_NAME_LEN - 1] = '\0';
                    count++;
                    ESP_LOGI(TAG, "Found MP3: %s", entry->d_name);
                }
            }
        }
    }

    closedir(dir);
    ESP_LOGI(TAG, "Found %d MP3 files", count);
    return count;
}

long sd_card_get_file_size(const char *filepath)
{
    struct stat st;
    if (stat(filepath, &st) == 0) {
        return st.st_size;
    }
    return -1;
}
