/**
 * @file main.c
 * @brief Level-2 MP3播放器 - Flash存储版本
 *
 * 使用ESP32-S3-N8R8开发板 + MAX98357AETE音频功放
 * 从SPIFFS文件系统读取并播放MP3文件
 *
 * 硬件连接：
 * - MAX98357A WS/LRC  -> GPIO5
 * - MAX98357A BCLK    -> GPIO6
 * - MAX98357A DIN     -> GPIO7
 * - MAX98357A SD      -> GPIO18 (高电平使能)
 * - MAX98357A GND     -> GND
 * - MAX98357A VIN     -> 3.3V
 *
 * 功能说明：
 * - 启动时自动扫描SPIFFS中的MP3文件
 * - 自动播放所有MP3文件
 * - 串口命令控制：播放/暂停/停止/下一首/上一首/音量
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "include/i2s_driver.h"
#include "include/mp3_player.h"

static const char *TAG = "MAIN";

/* 播放器句柄 */
static mp3_player_handle_t g_player = NULL;

/**
 * @brief 测试I2S输出 - 生成1kHz正弦波
 */
static void test_tone(void)
{
    ESP_LOGI(TAG, "========== 测试1kHz正弦波 ==========");

    i2s_driver_handle_t i2s = mp3_player_get_i2s_handle(g_player);
    if (!i2s) {
        ESP_LOGE(TAG, "无法获取I2S句柄");
        return;
    }

    /* 使用44100Hz采样率，与MP3一致 */
    int sample_rate = 44100;
    int samples_per_10ms = sample_rate / 100;  /* 441 samples per 10ms */

    int16_t *tone_buffer = malloc(samples_per_10ms * sizeof(int16_t) * 2);  /* 10ms, stereo */
    if (!tone_buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }

    /* 生成1kHz正弦波，44100Hz采样率，立体声 */
    for (int i = 0; i < samples_per_10ms; i++) {
        int16_t sample = (int16_t)(8000 * sin(2 * M_PI * 1000 * i / sample_rate));
        tone_buffer[i * 2] = sample;      /* 左声道 */
        tone_buffer[i * 2 + 1] = sample;  /* 右声道 */
    }

    ESP_LOGI(TAG, "开始播放测试音5秒...");
    /* 暂停播放器 */
    mp3_player_stop(g_player);

    for (int j = 0; j < 500; j++) {  /* 5秒 */
        i2s_driver_write(i2s, tone_buffer, samples_per_10ms, portMAX_DELAY);
    }

    free(tone_buffer);
    ESP_LOGI(TAG, "测试音结束");
}
static void print_playlist(void)
{
    int count = mp3_player_get_file_count(g_player);

    ESP_LOGI(TAG, "========== 播放列表 ==========");
    ESP_LOGI(TAG, "共 %d 首歌曲", count);

    int current = mp3_player_get_current_index(g_player);

    for (int i = 0; i < count; i++) {
        const char *name = mp3_player_get_file_name(g_player, i);
        if (name) {
            ESP_LOGI(TAG, " %c %d. %s", (i == current) ? '>' : ' ', i + 1, name);
        }
    }
    ESP_LOGI(TAG, "==============================");
}

/**
 * @brief 播放器事件回调
 */
static void player_event_callback(mp3_player_event_t event, int track_index, void *user_data)
{
    (void)user_data;

    switch (event) {
        case MP3_EVENT_TRACK_STARTED:
            ESP_LOGI(TAG, "[事件] 开始播放: %s",
                     mp3_player_get_file_name(g_player, track_index));
            break;

        case MP3_EVENT_TRACK_FINISHED:
            ESP_LOGI(TAG, "[事件] 播放完成: %s",
                     mp3_player_get_file_name(g_player, track_index));
            break;

        case MP3_EVENT_ERROR:
            ESP_LOGE(TAG, "[事件] 播放错误");
            break;

        default:
            break;
    }
}

/**
 * @brief 打印帮助信息
 */
static void print_help(void)
{
    printf("\n========== MP3播放器命令 ==========\n");
    printf("  p     - 播放/暂停\n");
    printf("  s     - 停止\n");
    printf("  n     - 下一首\n");
    printf("  b     - 上一首\n");
    printf("  l     - 显示播放列表\n");
    printf("  t     - 测试1kHz正弦波\n");
    printf("  v+    - 增加音量\n");
    printf("  v-    - 减小音量\n");
    printf("  vXX   - 设置音量为XX (0-100)\n");
    printf("  1-9   - 播放指定曲目\n");
    printf("  h     - 显示帮助\n");
    printf("====================================\n\n");
}

/**
 * @brief 显示当前状态
 */
static void print_status(void)
{
    mp3_player_state_t state = mp3_player_get_state(g_player);
    int current = mp3_player_get_current_index(g_player);
    uint8_t volume = mp3_player_get_volume(g_player);

    const char *state_str = (state == MP3_STATE_PLAYING) ? "播放中" :
                            (state == MP3_STATE_PAUSED) ? "已暂停" : "已停止";

    printf("\n[状态] %s | 曲目: %d/%d | 音量: %d%%\n",
           state_str,
           current + 1,
           mp3_player_get_file_count(g_player),
           volume);

    if (current >= 0) {
        printf("[当前] %s\n", mp3_player_get_file_name(g_player, current));
    }
}

/**
 * @brief 处理串口命令
 */
static void process_command(const char *cmd)
{
    if (cmd == NULL || strlen(cmd) == 0) {
        return;
    }

    char c = tolower((unsigned char)cmd[0]);

    switch (c) {
        case 'p':  /* 播放/暂停 */
            if (mp3_player_get_state(g_player) == MP3_STATE_PLAYING) {
                mp3_player_pause(g_player);
                printf("已暂停\n");
            } else if (mp3_player_get_state(g_player) == MP3_STATE_PAUSED) {
                mp3_player_resume(g_player);
                printf("继续播放\n");
            } else {
                int count = mp3_player_get_file_count(g_player);
                if (count > 0) {
                    int idx = mp3_player_get_current_index(g_player);
                    if (idx < 0) idx = 0;
                    mp3_player_play(g_player, idx);
                }
            }
            break;

        case 's':  /* 停止 */
            mp3_player_stop(g_player);
            printf("已停止\n");
            break;

        case 'n':  /* 下一首 */
            mp3_player_next(g_player);
            break;

        case 'b':  /* 上一首 */
            mp3_player_prev(g_player);
            break;

        case 'l':  /* 显示播放列表 */
            print_playlist();
            break;

        case 't':  /* 测试1kHz正弦波 */
            test_tone();
            break;

        case 'v':  /* 音量控制 */
            if (strlen(cmd) >= 2) {
                if (cmd[1] == '+') {
                    uint8_t vol = mp3_player_get_volume(g_player);
                    vol = (vol + 10 > 100) ? 100 : vol + 10;
                    mp3_player_set_volume(g_player, vol);
                    printf("音量: %d%%\n", vol);
                } else if (cmd[1] == '-') {
                    uint8_t vol = mp3_player_get_volume(g_player);
                    vol = (vol < 10) ? 0 : vol - 10;
                    mp3_player_set_volume(g_player, vol);
                    printf("音量: %d%%\n", vol);
                } else {
                    int vol = atoi(cmd + 1);
                    if (vol >= 0 && vol <= 100) {
                        mp3_player_set_volume(g_player, (uint8_t)vol);
                        printf("音量: %d%%\n", vol);
                    }
                }
            }
            break;

        case 'h':  /* 帮助 */
        case '?':
            print_help();
            break;

        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            /* 播放指定曲目 */
            int idx = atoi(cmd) - 1;
            int count = mp3_player_get_file_count(g_player);
            if (idx >= 0 && idx < count) {
                mp3_player_play(g_player, idx);
            } else {
                printf("无效曲目编号: %d (范围: 1-%d)\n", idx + 1, count);
            }
            break;
        }

        default:
            printf("未知命令: '%c'，输入 'h' 查看帮助\n", c);
            break;
    }

    print_status();
}

/**
 * @brief 串口读取任务
 */
static void uart_task(void *pvParameters)
{
    (void)pvParameters;

    char cmd_buf[64];
    int cmd_len = 0;

    ESP_LOGI(TAG, "串口命令任务启动");
    print_help();
    print_status();

    while (1) {
        int c = getchar();

        if (c != EOF && c != 0) {
            if (c == '\r' || c == '\n') {
                if (cmd_len > 0) {
                    cmd_buf[cmd_len] = '\0';
                    process_command(cmd_buf);
                    cmd_len = 0;
                }
            } else if (cmd_len < sizeof(cmd_buf) - 1 && isprint(c)) {
                cmd_buf[cmd_len++] = c;
                putchar(c);  // 回显字符
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief 主入口
 */
void app_main(void)
{
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "ESP32-S3 MP3播放器 - Flash版本");
    ESP_LOGI(TAG, "开发板: ESP32-S3-N8R8");
    ESP_LOGI(TAG, "功放: MAX98357AETE");
    ESP_LOGI(TAG, "================================");

    /* 等待系统稳定 */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 初始化MP3播放器 */
    mp3_player_config_t player_cfg = {
        .i2s_handle = NULL,  /* 播放器会自己创建I2S */
        .sample_rate = 44100,
        .volume = 80,
        .callback = player_event_callback,
        .user_data = NULL,
    };

    g_player = mp3_player_init(&player_cfg);
    if (g_player == NULL) {
        ESP_LOGE(TAG, "播放器初始化失败");
        return;
    }

    /* 扫描MP3文件 */
    int file_count = mp3_player_scan_files(g_player);
    if (file_count == 0) {
        ESP_LOGW(TAG, "未找到MP3文件，请确保已上传音频文件到SPIFFS");
    }

    /* 打印播放列表 */
    print_playlist();

    /* 启动播放器任务 */
    if (!mp3_player_start_task(g_player)) {
        ESP_LOGE(TAG, "启动播放器任务失败");
        mp3_player_deinit(g_player);
        return;
    }

    /* 如果有文件，自动开始播放 */
    if (file_count > 0) {
        vTaskDelay(pdMS_TO_TICKS(500));
        mp3_player_play(g_player, 0);
    }

    /* 创建串口命令任务 */
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 5, NULL);

    /* 主循环 - 监控状态 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* 可以在这里添加其他监控功能 */
    }
}
