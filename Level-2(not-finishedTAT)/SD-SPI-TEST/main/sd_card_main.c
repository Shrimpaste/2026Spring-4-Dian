/*
 * SD卡SPI读取挂载实验
 * 功能：通过SPI接口初始化、挂载SD卡，并进行基础读写测试
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "SD_CARD_TEST";

#define MOUNT_POINT     "/sdcard"
#define TEST_FILE       "/sdcard/test.txt"
#define TEST_CONTENT    "Hello SD Card! 你好SD卡！"

// SPI引脚定义 - ESP32S3 (可根据实际硬件修改)
#define PIN_MOSI        4
#define PIN_MISO        5
#define PIN_CLK         2
#define PIN_CS          8

#define MAX_RETRY       3       // 最大重试次数
#define INIT_DELAY_MS   200     // 初始化前延时(给SD卡复位时间)

/**
 * @brief SD卡硬件级复位 - 发送80个时钟脉冲
 * 关键: CS保持高电平，发送80个时钟让SD卡退出任何状态回到空闲
 */
static void sd_card_send_dummy_clocks(void)
{
    ESP_LOGI(TAG, "发送复位时钟序列...");

    // 先重置GPIO状态
    gpio_reset_pin(PIN_CS);
    gpio_reset_pin(PIN_CLK);
    gpio_reset_pin(PIN_MOSI);
    gpio_reset_pin(PIN_MISO);

    // 配置CS/CLK/MOSI为GPIO输出
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << PIN_CS) | (1ULL << PIN_CLK) | (1ULL << PIN_MOSI),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_cfg);

    // CS保持高电平（SD卡未选中）
    gpio_set_level(PIN_CS, 1);
    // MOSI保持高电平（SD协议要求）
    gpio_set_level(PIN_MOSI, 1);
    // CLK初始低电平
    gpio_set_level(PIN_CLK, 0);

    // 延时让SD卡稳定
    vTaskDelay(pdMS_TO_TICKS(50));

    // 发送至少74个时钟脉冲（SD协议要求），用更慢的速度
    for (int i = 0; i < 100; i++) {
        gpio_set_level(PIN_CLK, 0);
        esp_rom_delay_us(10);  // 50KHz频率
        gpio_set_level(PIN_CLK, 1);
        esp_rom_delay_us(10);
    }
    gpio_set_level(PIN_CLK, 0);

    // 继续拉高CS一段时间
    vTaskDelay(pdMS_TO_TICKS(200));

    // 释放GPIO，让SPI接管
    gpio_reset_pin(PIN_CS);
    gpio_reset_pin(PIN_CLK);
    gpio_reset_pin(PIN_MOSI);
    gpio_reset_pin(PIN_MISO);

    ESP_LOGI(TAG, "复位时钟序列完成");
}

/**
 * @brief 带重试的SD卡挂载，每次重试前发送复位时钟
 */
static esp_err_t sd_mount_with_retry(sdmmc_host_t *host,
                                      sdspi_device_config_t *slot_config,
                                      esp_vfs_fat_sdmmc_mount_config_t *mount_config,
                                      sdmmc_card_t **card,
                                      spi_bus_config_t *bus_cfg)
{
    esp_err_t ret;
    int retry = 0;

    while (retry < MAX_RETRY) {
        ESP_LOGI(TAG, "尝试挂载SD卡 (第 %d/%d 次)...", retry + 1, MAX_RETRY);

        ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, host, slot_config, mount_config, card);

        if (ret == ESP_OK) {
            return ESP_OK;
        }

        ESP_LOGW(TAG, "挂载失败: %s (0x%x)", esp_err_to_name(ret), ret);
        retry++;

        if (retry < MAX_RETRY) {
            ESP_LOGI(TAG, "重新发送复位时钟序列...");
            // 挂载失败后，SPI总线需要释放并重新初始化
            spi_bus_free(host->slot);
            vTaskDelay(pdMS_TO_TICKS(200));
            // 重新发送复位时钟
            sd_card_send_dummy_clocks();
            vTaskDelay(pdMS_TO_TICKS(100));
            // 重新初始化SPI总线
            ret = spi_bus_initialize(host->slot, bus_cfg, SDSPI_DEFAULT_DMA);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "SPI重新初始化失败: %s", esp_err_to_name(ret));
            }
        }
    }

    return ret;
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "========== SD卡SPI挂载实验 ==========");
    ESP_LOGI(TAG, "引脚配置: MOSI=%d, MISO=%d, CLK=%d, CS=%d", PIN_MOSI, PIN_MISO, PIN_CLK, PIN_CS);

    // 0. 发送复位时钟序列 (关键! 解决0x107错误)
    // SD卡需要至少74个时钟脉冲才能退出忙状态
    sd_card_send_dummy_clocks();

    // 1. 配置挂载参数
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,    // 挂载失败时不格式化
        .max_files = 4,                     // 最大打开文件数
        .allocation_unit_size = 16 * 1024   // 分配单元大小
    };

    // 2. 初始化SDMMC主机 (SPI模式)
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 100;  // 使用100KHz低速初始化，提高兼容性

    // 3. 配置SPI总线
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,    // 不使用Quad SPI
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096 // 最大传输大小
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI总线初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "SPI总线初始化成功");

    // 4. 配置SD设备
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_CS;
    slot_config.host_id = host.slot;

    // 5. 挂载文件系统 (带重试)
    sdmmc_card_t *card;
    ESP_LOGI(TAG, "正在挂载SD卡...");
    ret = sd_mount_with_retry(&host, &slot_config, &mount_config, &card, &bus_cfg);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "挂载失败: 文件系统无法挂载，建议格式化SD卡为FAT32格式");
        } else {
            ESP_LOGE(TAG, "初始化失败: %s (0x%x)", esp_err_to_name(ret), ret);
            ESP_LOGE(TAG, "请检查:\n  1. SD卡是否正确插入\n  2. 引脚连接是否正确\n  3. 是否已添加上拉电阻(建议10K)");
            if (ret == 0x107) {
                ESP_LOGE(TAG, "错误0x107=ESP_ERR_TIMEOUT: SD卡可能处于忙状态，\n建议: 重新插拔SD卡或完全断电重启");
            }
        }
        spi_bus_free(host.slot);
        return;
    }
    ESP_LOGI(TAG, "SD卡挂载成功!");

    // 6. 打印SD卡信息
    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "SD卡容量: %llu MB", ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024));

    // 7. 写入测试
    ESP_LOGI(TAG, "开始写入测试...");
    FILE *f = fopen(TEST_FILE, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "打开文件失败");
    } else {
        fprintf(f, "%s\n", TEST_CONTENT);
        fprintf(f, "写入时间戳: %lu\n", (unsigned long)esp_log_timestamp());
        fclose(f);
        ESP_LOGI(TAG, "写入成功: %s", TEST_FILE);
    }

    // 8. 读取测试
    ESP_LOGI(TAG, "开始读取测试...");
    f = fopen(TEST_FILE, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "读取文件失败");
    } else {
        char line[128];
        ESP_LOGI(TAG, "文件内容:");
        while (fgets(line, sizeof(line), f) != NULL) {
            // 去掉换行符
            line[strcspn(line, "\n")] = 0;
            ESP_LOGI(TAG, "  > %s", line);
        }
        fclose(f);
    }

    // 9. 列出根目录文件
    ESP_LOGI(TAG, "SD卡根目录文件列表:");
    DIR *dir = opendir(MOUNT_POINT);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "  %s", entry->d_name);
        }
        closedir(dir);
    }

    // 10. 卸载
    ESP_LOGI(TAG, "正在卸载SD卡...");
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    spi_bus_free(host.slot);
    ESP_LOGI(TAG, "SD卡已卸载，实验完成!");

    // 实验结束，进入空闲循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
