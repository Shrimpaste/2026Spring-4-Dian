/**
 * @file mp3_player.c
 * @brief MP3播放器模块实现
 *
 * 基于SPIFFS文件系统和minimp3解码器
 */

#include "include/mp3_player.h"
#include "include/i2s_driver.h"
#include "minimp3.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "dirent.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

static const char *TAG = "MP3_PLAYER";

#define MP3_DECODER_STACK_SIZE      8192
#define MP3_DECODER_TASK_PRIORITY   5
#define MP3_READ_BUFFER_SIZE        16384     /* 16KB MP3数据缓冲区 */
#define MP3_PCM_BUFFER_SAMPLES      1152 * 2  /* max samples per frame * channels */
#define MP3_FILE_PREFIX             "/spiffs/"
#define MP3_DECODE_BATCH_FRAMES     10        /* 每次连续解码的帧数 */

/**
 * @brief MP3播放器实例结构体
 */
typedef struct mp3_player_s {
    /* I2S */
    i2s_driver_handle_t i2s_handle;      /* I2S驱动句柄 */
    bool                i2s_owner;       /* 是否拥有I2S（需要释放） */

    /* 文件列表 */
    char                file_list[MP3_PLAYER_MAX_FILES][MP3_PLAYER_FILE_NAME_MAX];
    int                 file_count;      /* 文件总数 */
    int                 current_index;   /* 当前播放索引 */

    /* 播放状态 */
    mp3_player_state_t  state;           /* 播放状态 */
    uint8_t             volume;          /* 音量 0-100 */

    /* 解码器 */
    mp3dec_t            mp3dec;          /* minimp3解码器 */
    int16_t             *pcm_buffer;     /* PCM输出缓冲区 */
    uint8_t             *read_buffer;    /* MP3数据读取缓冲区 */
    int                 buffer_valid;    /* 缓冲区有效字节数 */
    int                 buffer_pos;      /* 当前解码位置 */

    /* 当前文件 */
    FILE                *current_file;   /* 当前打开的文件 */
    long                file_position;   /* 当前文件位置 */
    bool                file_eof;        /* 文件是否结束 */

    /* 回调 */
    mp3_player_callback_t callback;      /* 事件回调 */
    void                *user_data;      /* 用户数据 */

    /* 任务控制 */
    TaskHandle_t        task_handle;     /* 播放器任务句柄 */
    SemaphoreHandle_t   mutex;           /* 互斥锁 */
    bool                task_running;    /* 任务是否运行中 */
    bool                stop_requested;  /* 请求停止 */
} mp3_player_t;

/* 内部函数声明 */
static bool mp3_player_open_file(mp3_player_t *player, int index);
static void mp3_player_close_file(mp3_player_t *player);
static bool mp3_player_decode_frame(mp3_player_t *player);
static void mp3_player_task(void *param);
static void mp3_player_notify_event(mp3_player_t *player, mp3_player_event_t event, int track_index);

/**
 * @brief 初始化SPIFFS文件系统
 */
static bool init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return false;
    }

    /* 获取SPIFFS信息 */
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS: total: %d, used: %d", total, used);
    }

    return true;
}

/**
 * @brief 检查文件是否为MP3文件
 */
static bool is_mp3_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return false;
    }
    return strcasecmp(ext, ".mp3") == 0;
}

mp3_player_handle_t mp3_player_init(const mp3_player_config_t *config)
{
    /* 分配播放器实例 */
    mp3_player_t *player = (mp3_player_t *)calloc(1, sizeof(mp3_player_t));
    if (player == NULL) {
        ESP_LOGE(TAG, "Failed to allocate player memory");
        return NULL;
    }

    /* 初始化SPIFFS */
    if (!init_spiffs()) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS");
        free(player);
        return NULL;
    }

    /* 初始化I2S */
    if (config && config->i2s_handle) {
        player->i2s_handle = config->i2s_handle;
        player->i2s_owner = false;
    } else {
        i2s_driver_config_t i2s_cfg = I2S_DRIVER_DEFAULT_CONFIG();
        if (config) {
            i2s_cfg.sample_rate = config->sample_rate ? config->sample_rate : 44100;
        }
        player->i2s_handle = i2s_driver_init(&i2s_cfg);
        if (player->i2s_handle == NULL) {
            ESP_LOGE(TAG, "Failed to initialize I2S");
            free(player);
            return NULL;
        }
        player->i2s_owner = true;
    }

    /* 分配缓冲区 */
    player->pcm_buffer = (int16_t *)malloc(MP3_PCM_BUFFER_SAMPLES * sizeof(int16_t));
    player->read_buffer = (uint8_t *)malloc(MP3_READ_BUFFER_SIZE);

    if (player->pcm_buffer == NULL || player->read_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        free(player->pcm_buffer);
        free(player->read_buffer);
        if (player->i2s_owner) {
            i2s_driver_deinit(player->i2s_handle);
        }
        free(player);
        return NULL;
    }

    /* 初始化解码器 */
    mp3dec_init(&player->mp3dec);

    /* 初始化状态 */
    player->state = MP3_STATE_STOPPED;
    player->volume = (config && config->volume) ? config->volume : 80;
    player->current_index = -1;
    player->file_count = 0;
    player->buffer_valid = 0;
    player->buffer_pos = 0;
    player->file_eof = false;
    player->callback = config ? config->callback : NULL;
    player->user_data = config ? config->user_data : NULL;

    /* 创建互斥锁 */
    player->mutex = xSemaphoreCreateMutex();
    if (player->mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(player->pcm_buffer);
        free(player->read_buffer);
        if (player->i2s_owner) {
            i2s_driver_deinit(player->i2s_handle);
        }
        free(player);
        return NULL;
    }

    ESP_LOGI(TAG, "MP3 player initialized, volume: %d%%", player->volume);

    return (mp3_player_handle_t)player;
}

void mp3_player_deinit(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    /* 停止任务 */
    mp3_player_stop_task(handle);

    /* 关闭文件 */
    mp3_player_close_file(player);

    /* 释放I2S */
    if (player->i2s_owner && player->i2s_handle) {
        i2s_driver_deinit(player->i2s_handle);
    }

    /* 释放缓冲区 */
    free(player->pcm_buffer);
    free(player->read_buffer);

    /* 删除互斥锁 */
    if (player->mutex) {
        vSemaphoreDelete(player->mutex);
    }

    /* 卸载SPIFFS */
    esp_vfs_spiffs_unregister(NULL);

    free(player);

    ESP_LOGI(TAG, "MP3 player deinitialized");
}

int mp3_player_scan_files(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    xSemaphoreTake(player->mutex, portMAX_DELAY);

    DIR *dir = opendir("/spiffs");
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open /spiffs directory");
        xSemaphoreGive(player->mutex);
        return 0;
    }

    player->file_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL && player->file_count < MP3_PLAYER_MAX_FILES) {
        if (entry->d_type == DT_REG && is_mp3_file(entry->d_name)) {
            strncpy(player->file_list[player->file_count], entry->d_name,
                    MP3_PLAYER_FILE_NAME_MAX - 1);
            player->file_list[player->file_count][MP3_PLAYER_FILE_NAME_MAX - 1] = '\0';
            ESP_LOGI(TAG, "Found MP3: %s", player->file_list[player->file_count]);
            player->file_count++;
        }
    }

    closedir(dir);

    /* 按文件名排序 */
    for (int i = 0; i < player->file_count - 1; i++) {
        for (int j = i + 1; j < player->file_count; j++) {
            if (strcasecmp(player->file_list[i], player->file_list[j]) > 0) {
                char temp[MP3_PLAYER_FILE_NAME_MAX];
                strcpy(temp, player->file_list[i]);
                strcpy(player->file_list[i], player->file_list[j]);
                strcpy(player->file_list[j], temp);
            }
        }
    }

    xSemaphoreGive(player->mutex);

    ESP_LOGI(TAG, "Total MP3 files: %d", player->file_count);
    return player->file_count;
}

int mp3_player_get_file_count(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }
    mp3_player_t *player = (mp3_player_t *)handle;
    return player->file_count;
}

const char* mp3_player_get_file_name(mp3_player_handle_t handle, int index)
{
    if (handle == NULL || index < 0 || index >= ((mp3_player_t *)handle)->file_count) {
        return NULL;
    }
    mp3_player_t *player = (mp3_player_t *)handle;
    return player->file_list[index];
}

static bool mp3_player_open_file(mp3_player_t *player, int index)
{
    if (player == NULL || index < 0 || index >= player->file_count) {
        return false;
    }

    /* 关闭之前的文件 */
    mp3_player_close_file(player);

    /* 构建完整路径 */
    char path[MP3_PLAYER_FILE_NAME_MAX + 16];
    snprintf(path, sizeof(path), "%s%s", MP3_FILE_PREFIX, player->file_list[index]);

    ESP_LOGI(TAG, "Opening file: %s", path);

    player->current_file = fopen(path, "rb");
    if (player->current_file == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return false;
    }

    player->current_index = index;
    player->file_position = 0;
    player->buffer_valid = 0;
    player->buffer_pos = 0;
    player->file_eof = false;

    return true;
}

static void mp3_player_close_file(mp3_player_t *player)
{
    if (player->current_file) {
        fclose(player->current_file);
        player->current_file = NULL;
    }
}

static void mp3_player_notify_event(mp3_player_t *player, mp3_player_event_t event, int track_index)
{
    if (player->callback) {
        player->callback(event, track_index, player->user_data);
    }
}

bool mp3_player_play(mp3_player_handle_t handle, int index)
{
    if (handle == NULL) {
        return false;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    xSemaphoreTake(player->mutex, portMAX_DELAY);

    if (!mp3_player_open_file(player, index)) {
        xSemaphoreGive(player->mutex);
        return false;
    }

    player->state = MP3_STATE_PLAYING;
    player->stop_requested = false;

    /* 重新初始化解码器 */
    mp3dec_init(&player->mp3dec);

    xSemaphoreGive(player->mutex);

    ESP_LOGI(TAG, "Playing: %s", player->file_list[index]);
    mp3_player_notify_event(player, MP3_EVENT_TRACK_STARTED, index);

    return true;
}

bool mp3_player_pause(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    xSemaphoreTake(player->mutex, portMAX_DELAY);

    if (player->state == MP3_STATE_PLAYING) {
        player->state = MP3_STATE_PAUSED;
        i2s_driver_pause(player->i2s_handle);
        ESP_LOGI(TAG, "Paused");
    }

    xSemaphoreGive(player->mutex);

    return true;
}

bool mp3_player_resume(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    xSemaphoreTake(player->mutex, portMAX_DELAY);

    if (player->state == MP3_STATE_PAUSED) {
        player->state = MP3_STATE_PLAYING;
        i2s_driver_resume(player->i2s_handle);
        ESP_LOGI(TAG, "Resumed");
    }

    xSemaphoreGive(player->mutex);

    return true;
}

bool mp3_player_stop(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    xSemaphoreTake(player->mutex, portMAX_DELAY);

    player->state = MP3_STATE_STOPPED;
    player->stop_requested = true;
    mp3_player_close_file(player);
    i2s_driver_pause(player->i2s_handle);

    xSemaphoreGive(player->mutex);

    ESP_LOGI(TAG, "Stopped");
    return true;
}

bool mp3_player_next(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    if (player->file_count == 0) {
        return false;
    }

    int next_index = (player->current_index + 1) % player->file_count;
    return mp3_player_play(handle, next_index);
}

bool mp3_player_prev(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    if (player->file_count == 0) {
        return false;
    }

    int prev_index = (player->current_index - 1 + player->file_count) % player->file_count;
    return mp3_player_play(handle, prev_index);
}

mp3_player_state_t mp3_player_get_state(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return MP3_STATE_STOPPED;
    }
    return ((mp3_player_t *)handle)->state;
}

int mp3_player_get_current_index(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return -1;
    }
    return ((mp3_player_t *)handle)->current_index;
}

void mp3_player_set_volume(mp3_player_handle_t handle, uint8_t volume)
{
    if (handle == NULL) {
        return;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    if (volume > 100) {
        volume = 100;
    }

    player->volume = volume;
    ESP_LOGI(TAG, "Volume set to %d%%", volume);
}

uint8_t mp3_player_get_volume(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }
    return ((mp3_player_t *)handle)->volume;
}

i2s_driver_handle_t mp3_player_get_i2s_handle(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return NULL;
    }
    return ((mp3_player_t *)handle)->i2s_handle;
}

/**
 * @brief 应用音量到PCM数据
 */
static void apply_volume(int16_t *buffer, int samples, uint8_t volume)
{
    if (volume == 100) {
        return;
    }

    for (int i = 0; i < samples; i++) {
        buffer[i] = (int16_t)(((int32_t)buffer[i] * volume) / 100);
    }
}

/**
 * @brief 填充MP3缓冲区
 * @return 实际读取的字节数
 */
static int mp3_player_fill_buffer(mp3_player_t *player)
{
    if (player->current_file == NULL || player->file_eof) {
        return 0;
    }

    /* 将未解码数据移到缓冲区开头 */
    int remaining = player->buffer_valid - player->buffer_pos;
    if (remaining > 0 && player->buffer_pos > 0) {
        memmove(player->read_buffer, player->read_buffer + player->buffer_pos, remaining);
    }

    /* 读取新数据填充缓冲区 */
    int space = MP3_READ_BUFFER_SIZE - remaining;
    int bytes_read = 0;

    if (space > 0) {
        bytes_read = fread(player->read_buffer + remaining, 1, space, player->current_file);
        if (bytes_read < space) {
            player->file_eof = true;
        }
    }

    player->buffer_valid = remaining + bytes_read;
    player->buffer_pos = 0;

    return bytes_read;
}

/**
 * @brief 解码并播放一帧MP3数据
 * @return true=成功继续, false=文件结束或错误
 */
static bool mp3_player_decode_frame(mp3_player_t *player)
{
    if (player->current_file == NULL) {
        return false;
    }

    /* 确保缓冲区有足够数据 */
    if (player->buffer_valid - player->buffer_pos < 4096) {
        mp3_player_fill_buffer(player);
    }

    /* 检查是否有足够数据解码 */
    int available = player->buffer_valid - player->buffer_pos;
    if (available < 16) {
        /* 缓冲区数据不足，尝试最后一次填充 */
        mp3_player_fill_buffer(player);
        available = player->buffer_valid - player->buffer_pos;
        if (available < 16) {
            return false;  /* 文件结束 */
        }
    }

    /* 解码MP3帧 - 从RAM缓冲区读取，不访问SPI flash */
    mp3dec_frame_info_t frame_info;
    int samples = mp3dec_decode_frame(&player->mp3dec,
                                       player->read_buffer + player->buffer_pos,
                                       available,
                                       player->pcm_buffer,
                                       &frame_info);

    if (samples > 0) {
        /* 应用音量 */
        int total_samples = samples * frame_info.channels;
        apply_volume(player->pcm_buffer, total_samples, player->volume);

        /* 调试：输出音频数据前几个样本 */
        static int frame_count = 0;
        if (++frame_count % 100 == 0) {
            ESP_LOGI(TAG, "解码帧 #%d: %d samples, %d ch, %d Hz, %d kbps",
                     frame_count, samples, frame_info.channels,
                     frame_info.hz, frame_info.bitrate_kbps);
            ESP_LOGI(TAG, "音频样本前4个: %d %d %d %d",
                     player->pcm_buffer[0], player->pcm_buffer[1],
                     player->pcm_buffer[2], player->pcm_buffer[3]);
        }

        /* 输出到I2S */
        int samples_written = i2s_driver_write(player->i2s_handle,
                                                player->pcm_buffer,
                                                samples,
                                                portMAX_DELAY);

        if (samples_written < 0) {
            ESP_LOGE(TAG, "I2S write failed");
            return false;
        }

        /* 移动缓冲区位置 */
        if (frame_info.frame_bytes > 0) {
            player->buffer_pos += frame_info.frame_bytes;
        } else {
            /* 帧解析失败，跳过一字节 */
            player->buffer_pos += 1;
        }
    } else {
        /* 没有解码出有效帧，跳过一字节继续 */
        player->buffer_pos += 1;
    }

    return true;
}

bool mp3_player_loop(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    if (player->state != MP3_STATE_PLAYING) {
        return false;
    }

    if (!mp3_player_decode_frame(player)) {
        /* 文件播放完成 */
        mp3_player_close_file(player);
        player->state = MP3_STATE_STOPPED;
        mp3_player_notify_event(player, MP3_EVENT_TRACK_FINISHED, player->current_index);
        return false;
    }

    return true;
}

static void mp3_player_task(void *param)
{
    mp3_player_t *player = (mp3_player_t *)param;

    ESP_LOGI(TAG, "Player task started on Core %d", xPortGetCoreID());

    int frames_decoded = 0;

    while (player->task_running) {
        if (player->state == MP3_STATE_PLAYING) {
            if (!mp3_player_decode_frame(player)) {
                /* 当前曲目播放完成，尝试播放下一首 */
                if (!player->stop_requested && player->file_count > 0) {
                    vTaskDelay(pdMS_TO_TICKS(500));  /* 间隔500ms */
                    int next_index = (player->current_index + 1) % player->file_count;
                    mp3_player_play((mp3_player_handle_t)player, next_index);
                }
                frames_decoded = 0;
            } else {
                frames_decoded++;
                /* 每解码N帧才让出CPU，减少SPI flash访问频率 */
                if (frames_decoded >= MP3_DECODE_BATCH_FRAMES) {
                    vTaskDelay(pdMS_TO_TICKS(1));
                    frames_decoded = 0;
                }
                /* 添加taskYIELD确保低优先级任务有机会运行 */
                if (frames_decoded % 3 == 0) {
                    taskYIELD();
                }
            }
        } else {
            frames_decoded = 0;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    ESP_LOGI(TAG, "Player task stopped");
    player->task_handle = NULL;
    vTaskDelete(NULL);
}

bool mp3_player_start_task(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    if (player->task_handle != NULL) {
        ESP_LOGW(TAG, "Task already running");
        return true;
    }

    player->task_running = true;

    BaseType_t ret = xTaskCreatePinnedToCore(mp3_player_task,
                                  "mp3_player",
                                  MP3_DECODER_STACK_SIZE,
                                  player,
                                  MP3_DECODER_TASK_PRIORITY,
                                  &player->task_handle,
                                  1);  /* 固定到Core 1，避免SPI flash缓存冲突 */

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create player task");
        player->task_running = false;
        return false;
    }

    ESP_LOGI(TAG, "Player task created");
    return true;
}

void mp3_player_stop_task(mp3_player_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    mp3_player_t *player = (mp3_player_t *)handle;

    player->task_running = false;
    player->stop_requested = true;

    /* 等待任务结束 */
    if (player->task_handle != NULL) {
        int timeout = 100;
        while (player->task_handle != NULL && timeout-- > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
