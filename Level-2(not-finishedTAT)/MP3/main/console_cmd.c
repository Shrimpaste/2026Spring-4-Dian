/**
 * @file console_cmd.c
 * @brief Console command implementation for serial control
 *
 * Commands:
 * - play [n]     : Play song (optional: song number)
 * - pause        : Pause playback
 * - resume       : Resume playback
 * - stop         : Stop playback
 * - next         : Next song
 * - prev         : Previous song
 * - list         : List all songs
 * - vol [n]      : Set volume (0-100)
 * - status       : Show player status
 * - name <name>  : Set player name
 * - help         : Show help
 */

#include "console_cmd.h"
#include "mp3_player.h"
#include "sd_card.h"
#include "i2s_audio.h"
#include "driver/uart.h"
#include <string.h>

static const char *TAG = "CONSOLE";

static bool s_console_running = false;
static char s_input_buffer[256];

static void trim_newline(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

static char* trim_whitespace(char *str)
{
    while (*str == ' ' || *str == '\t') str++;
    return str;
}

esp_err_t console_cmd_init(void)
{
    /* Configure UART for console input */
    esp_err_t ret = uart_driver_install(UART_NUM_0, 256, 256, 0, NULL, 0);
    if (ret != ESP_OK) {
        /* Driver may already be installed by file_transfer, that's OK */
        ESP_LOGI(TAG, "UART driver install returned %s, using existing driver", esp_err_to_name(ret));
    }

    /* Set UART parameters for console (115200 baud, 8N1) */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_NUM_0, &uart_config);

    s_console_running = true;
    ESP_LOGI(TAG, "Console initialized");
    return ESP_OK;
}

void console_cmd_print_welcome(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║          ESP32-S3 MP3 Player                            ║\n");
    printf("║          Serial Console Control                          ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");

    /* Check if first boot */
    if (g_player.config.is_first_boot) {
        printf("⚠️  First boot detected!\n");
        printf("Please set a name for your MP3 player using:\n");
        printf("  name <your-player-name>\n\n");
    } else {
        printf("🎵 Welcome back, %s!\n", g_player.config.player_name);
    }

    printf("Type 'help' for available commands.\n");
    printf("\n");
}

void console_cmd_print_help(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║ Available Commands:                                      ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  play [n]     - Play song (n = song number, optional)    ║\n");
    printf("║  pause        - Pause playback                           ║\n");
    printf("║  resume       - Resume playback                          ║\n");
    printf("║  stop         - Stop playback                            ║\n");
    printf("║  next         - Play next song                           ║\n");
    printf("║  prev         - Play previous song                       ║\n");
    printf("║  list         - List all songs in SD card                ║\n");
    printf("║  vol [0-100]  - Set volume level                         ║\n");
    printf("║  status       - Show player status                       ║\n");
    printf("║  storage      - Show storage info (SD card)              ║\n");
    printf("║  name <name>  - Set player name                          ║\n");
    printf("║  help         - Show this help message                   ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void console_cmd_print_status(void)
{
    char state_str[16];
    mp3_player_get_state_string(state_str, sizeof(state_str));

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║ Player Status:                                           ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║  Player Name: %-42s ║\n", g_player.config.player_name);
    printf("║  State:       %-42s ║\n", state_str);
    printf("║  Volume:     %d%%                                         ║\n", i2s_audio_get_volume());
    printf("║  Songs:      %d                                           ║\n", g_player.song_count);
    printf("║  Current:    %-42s ║\n", mp3_player_get_current_song_name());
    printf("║  Position:   %d bytes                                    ║\n", mp3_player_get_current_position());
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void console_cmd_print_songs(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║ Song List (%d songs):                                    ║\n", g_player.song_count);
    printf("╠══════════════════════════════════════════════════════════╣\n");

    if (g_player.song_count == 0) {
        printf("║ No MP3 files found in Flash storage!                     ║\n");
        printf("║ Use 'transfer' command or file transfer protocol to      ║\n");
        printf("║ upload MP3 files via UART.                               ║\n");
    } else {
        for (int i = 0; i < g_player.song_count; i++) {
            const char *marker = (i == g_player.playback.current_song_index) ? "▶ " : "  ";
            printf("║ %s%2d. %-49s ║\n", marker, i + 1, g_player.song_list[i].display_name);
        }
    }

    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void console_cmd_process(const char *cmdline)
{
    if (cmdline == NULL || strlen(cmdline) == 0) {
        return;
    }

    /* Copy to buffer for parsing */
    strncpy(s_input_buffer, cmdline, sizeof(s_input_buffer) - 1);
    s_input_buffer[sizeof(s_input_buffer) - 1] = '\0';

    trim_newline(s_input_buffer);

    char *cmd = trim_whitespace(s_input_buffer);
    char *arg = strchr(cmd, ' ');
    if (arg) {
        *arg = '\0';
        arg = trim_whitespace(arg + 1);
    }

    /* Process commands */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
        console_cmd_print_help();
    }
    else if (strcmp(cmd, "status") == 0 || strcmp(cmd, "s") == 0) {
        console_cmd_print_status();
    }
    else if (strcmp(cmd, "list") == 0 || strcmp(cmd, "ls") == 0) {
        console_cmd_print_songs();
    }
    else if (strcmp(cmd, "play") == 0 || strcmp(cmd, "p") == 0) {
        if (arg && strlen(arg) > 0) {
            int song_num = atoi(arg) - 1;
            if (song_num >= 0 && song_num < g_player.song_count) {
                mp3_player_send_cmd(CMD_PLAY, song_num);
                printf("▶ Playing song #%d: %s\n", song_num + 1,
                       g_player.song_list[song_num].display_name);
            } else {
                printf("❌ Invalid song number. Range: 1-%d\n", g_player.song_count);
            }
        } else {
            mp3_player_send_cmd(CMD_PLAY, -1);
            printf("▶ Resuming playback\n");
        }
    }
    else if (strcmp(cmd, "pause") == 0) {
        mp3_player_send_cmd(CMD_PAUSE, 0);
        printf("⏸ Paused\n");
    }
    else if (strcmp(cmd, "resume") == 0 || strcmp(cmd, "r") == 0) {
        mp3_player_send_cmd(CMD_RESUME, 0);
        printf("▶ Resumed\n");
    }
    else if (strcmp(cmd, "stop") == 0) {
        mp3_player_send_cmd(CMD_STOP, 0);
        printf("⏹ Stopped\n");
    }
    else if (strcmp(cmd, "next") == 0 || strcmp(cmd, "n") == 0) {
        mp3_player_send_cmd(CMD_NEXT, 0);
        printf("⏭ Next song\n");
    }
    else if (strcmp(cmd, "prev") == 0 || strcmp(cmd, "b") == 0) {
        mp3_player_send_cmd(CMD_PREV, 0);
        printf("⏮ Previous song\n");
    }
    else if (strcmp(cmd, "vol") == 0 || strcmp(cmd, "v") == 0) {
        if (arg && strlen(arg) > 0) {
            int vol = atoi(arg);
            if (vol >= 0 && vol <= 100) {
                mp3_player_send_cmd(CMD_SET_VOLUME, vol);
                printf("🔊 Volume set to %d%%\n", vol);
            } else {
                printf("❌ Volume must be between 0-100\n");
            }
        } else {
            printf("🔊 Current volume: %d%%\n", i2s_audio_get_volume());
        }
    }
    else if (strcmp(cmd, "name") == 0) {
        if (arg && strlen(arg) > 0) {
            if (strlen(arg) < MAX_PLAYER_NAME_LEN) {
                config_set_player_name(arg);
                printf("✅ Player name set to: %s\n", arg);
            } else {
                printf("❌ Name too long (max %d characters)\n", MAX_PLAYER_NAME_LEN - 1);
            }
        } else {
            printf("❌ Usage: name <your-player-name>\n");
        }
    }
    else if (strcmp(cmd, "storage") == 0 || strcmp(cmd, "info") == 0) {
        sd_card_print_info();
        printf("\n Songs in library: %d\n", g_player.song_count);
    }
    else if (strlen(cmd) > 0) {
        printf("❌ Unknown command: %s\n", cmd);
        printf("   Type 'help' for available commands.\n");
    }
}

static void console_task(void *pvParameters)
{
    uint8_t data;
    char line_buffer[256];
    int line_pos = 0;

    printf("\nmp3> ");
    fflush(stdout);

    while (s_console_running) {
        int len = uart_read_bytes(UART_NUM_0, &data, 1, pdMS_TO_TICKS(100));

        if (len > 0) {
            /* Echo character */
            putchar(data);
            fflush(stdout);

            if (data == '\r' || data == '\n') {
                /* Process line */
                line_buffer[line_pos] = '\0';
                printf("\n");

                if (line_pos > 0) {
                    console_cmd_process(line_buffer);
                }

                line_pos = 0;
                printf("mp3> ");
                fflush(stdout);
            }
            else if (data == '\b' || data == 0x7F) {
                /* Backspace */
                if (line_pos > 0) {
                    line_pos--;
                    printf("\b \b");
                    fflush(stdout);
                }
            }
            else if (line_pos < sizeof(line_buffer) - 1) {
                /* Add character to buffer */
                if (data >= 32 && data < 127) {
                    line_buffer[line_pos++] = data;
                }
            }
        }
        /* Small delay to prevent watchdog issues and allow other tasks to run */
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
}

esp_err_t console_cmd_start(void)
{
    xTaskCreate(console_task, "console", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "Console task started");
    return ESP_OK;
}

esp_err_t console_cmd_deinit(void)
{
    s_console_running = false;
    return ESP_OK;
}
