/**
 * @file main.c
 * @brief ESP32-S3 MP3 Player main application with SD Card Storage
 *
 * Hardware Configuration:
 * - ESP32-S3 Development Board (N8R8 - 8MB Flash, 8MB PSRAM)
 * - SD Card Storage: SPI interface
 *   * GPIO4:  MOSI (SD_CMD/DI)
 *   * GPIO5:  MISO (SD_DO/DO)
 *   * GPIO2:  SCK/CLK
 *   * GPIO8:  CS
 * - MAX98357A I2S Amplifier Module:
 *   * GPIO10: BCLK
 *   * GPIO11: LRCK/WS
 *   * GPIO6:  DATA/DIN
 *   * GPIO7:  SD_MODE (Shutdown control, active high)
 *   * VCC:    3.3V or 5V
 *   * GND:    GND
 *
 * Note: GPIO19 and GPIO20 are reserved for USB on ESP32-S3
 */

#include "mp3_player.h"
#include "i2s_audio.h"
#include "console_cmd.h"
#include "nvs_flash.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "ESP32-S3 MP3 Player Starting...");
    ESP_LOGI(TAG, "=================================");

    /* Initialize MP3 player (includes SD card, I2S, NVS) */
    ESP_ERROR_CHECK(mp3_player_init());

    /* Start console for serial control */
    ESP_ERROR_CHECK(console_cmd_init());

    /* Print welcome message */
    console_cmd_print_welcome();

    /* Start player tasks */
    ESP_ERROR_CHECK(mp3_player_start());

    /* Start console task */
    ESP_ERROR_CHECK(console_cmd_start());

    /* Show initial status */
    console_cmd_print_status();

    /* Show song list */
    console_cmd_print_songs();

    /* Check for first boot and prompt for name */
    if (g_player.config.is_first_boot) {
        printf("\n");
        printf("╔══════════════════════════════════════════════════════════╗\n");
        printf("║  First Boot - Please Name Your MP3 Player                ║\n");
        printf("╠══════════════════════════════════════════════════════════╣\n");
        printf("║  Enter a name for your MP3 player:                       ║\n");
        printf("╚══════════════════════════════════════════════════════════╝\n");
        printf("\nmp3> ");
        fflush(stdout);
    }

    ESP_LOGI(TAG, "MP3 Player ready!");

    /* Main loop - watchdog feed and status monitoring */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        /* Feed watchdog if needed */
        /* esp_task_wdt_reset(); */
    }
}
