/**
 * @file mp3_player.c
 * @brief MP3 Player core implementation
 */

#include "mp3_player.h"
#include "sd_card.h"
#include "i2s_audio.h"

/* minimp3 MP3 decoder */
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#ifndef SD_MOUNT_POINT
#define SD_MOUNT_POINT "/sdcard"
#endif
#include "nvs_flash.h"
#include "nvs.h"
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

static const char *TAG = "MP3_PLAYER";

mp3_player_t g_player = {0};

/* Command structure for queue */
typedef struct {
    player_cmd_t cmd;
    int param;
} cmd_msg_t;

/* Internal functions */
static void mp3_player_task(void *pvParameters);
static esp_err_t play_song(int index, uint32_t resume_pos);
static esp_err_t load_song_list_from_storage(void);
static void playback_task(void *pvParameters);

esp_err_t mp3_player_init(void)
{
    ESP_LOGI(TAG, "Initializing MP3 player...");

    memset(&g_player, 0, sizeof(g_player));
    g_player.state = STATE_IDLE;
    g_player.config.volume = 70;

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Load configuration */
    ESP_ERROR_CHECK(config_load(&g_player.config));

    /* Create command queue */
    g_player.cmd_queue = xQueueCreate(10, sizeof(cmd_msg_t));
    if (g_player.cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue");
        return ESP_ERR_NO_MEM;
    }

    /* Create state mutex */
    g_player.state_mutex = xSemaphoreCreateMutex();
    if (g_player.state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Initialize SD card */
    ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card initialization failed: %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "Continuing without SD card support");
    }

    /* Initialize I2S audio */
    ESP_ERROR_CHECK(i2s_audio_init());
    ESP_ERROR_CHECK(i2s_audio_set_volume(g_player.config.volume));

    /* Load song list - only if SD card is mounted */
    if (sd_card_is_mounted()) {
        load_song_list_from_storage();
        /* Load saved playback state */
        config_load_playback_state(&g_player.playback.current_song_index,
                                   &g_player.playback.resume_position);
    } else {
        ESP_LOGW(TAG, "No SD card mounted, song list empty");
    }

    ESP_LOGI(TAG, "MP3 player initialized");
    return ESP_OK;
}

esp_err_t mp3_player_start(void)
{
    /* Create player task */
    xTaskCreate(mp3_player_task, "mp3_player", 4096, NULL, 5, NULL);

    /* Create playback task */
    xTaskCreate(playback_task, "playback", 8192, NULL, 6, NULL);

    ESP_LOGI(TAG, "MP3 player started");
    return ESP_OK;
}

esp_err_t mp3_player_send_cmd(player_cmd_t cmd, int param)
{
    if (g_player.cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    cmd_msg_t msg = {
        .cmd = cmd,
        .param = param
    };

    if (xQueueSend(g_player.cmd_queue, &msg, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void mp3_player_task(void *pvParameters)
{
    cmd_msg_t msg;

    ESP_LOGI(TAG, "MP3 player task started");

    while (1) {
        if (xQueueReceive(g_player.cmd_queue, &msg, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(g_player.state_mutex, portMAX_DELAY);

            switch (msg.cmd) {
                case CMD_PLAY:
                    if (msg.param >= 0 && msg.param < g_player.song_count) {
                        play_song(msg.param, 0);
                    } else {
                        /* Resume current song */
                        i2s_audio_resume();
                        g_player.state = STATE_PLAYING;
                    }
                    break;

                case CMD_PAUSE:
                    if (g_player.state == STATE_PLAYING) {
                        i2s_audio_pause();
                        g_player.state = STATE_PAUSED;
                        /* Save position for resume */
                        g_player.playback.resume_position = i2s_audio_get_position();
                        config_save_playback_state(g_player.playback.current_song_index,
                                                   g_player.playback.resume_position);
                    }
                    break;

                case CMD_RESUME:
                    if (g_player.state == STATE_PAUSED) {
                        i2s_audio_resume();
                        g_player.state = STATE_PLAYING;
                    }
                    break;

                case CMD_NEXT:
                    if (g_player.song_count > 0) {
                        int next = (g_player.playback.current_song_index + 1) % g_player.song_count;
                        play_song(next, 0);
                    }
                    break;

                case CMD_PREV:
                    if (g_player.song_count > 0) {
                        int prev = (g_player.playback.current_song_index - 1 + g_player.song_count) % g_player.song_count;
                        play_song(prev, 0);
                    }
                    break;

                case CMD_STOP:
                    i2s_audio_stop_playback();
                    g_player.state = STATE_STOPPED;
                    g_player.playback.resume_position = 0;
                    break;

                case CMD_SET_VOLUME:
                    i2s_audio_set_volume(msg.param);
                    g_player.config.volume = msg.param;
                    break;

                case CMD_SET_SONG:
                    if (msg.param >= 0 && msg.param < g_player.song_count) {
                        uint32_t resume_pos = g_player.playback.resume_position;
                        play_song(msg.param, resume_pos);
                    }
                    break;

                default:
                    break;
            }

            xSemaphoreGive(g_player.state_mutex);
        }
    }
}

static esp_err_t play_song(int index, uint32_t resume_pos)
{
    if (index < 0 || index >= g_player.song_count) {
        return ESP_ERR_INVALID_ARG;
    }

    song_info_t *song = &g_player.song_list[index];
    char filepath[MAX_FILE_NAME_LEN + 16];
    snprintf(filepath, sizeof(filepath), "%s/%s", SD_MOUNT_POINT, song->filename);

    ESP_LOGI(TAG, "Playing [%d/%d]: %s", index + 1, g_player.song_count, song->display_name);

    esp_err_t ret = i2s_audio_play_mp3(filepath, resume_pos);
    if (ret == ESP_OK) {
        g_player.playback.current_song_index = index;
        g_player.state = STATE_PLAYING;

        /* Reset resume position when starting new song */
        if (resume_pos == 0) {
            g_player.playback.resume_position = 0;
        }
    }

    return ret;
}

static esp_err_t load_song_list_from_storage(void)
{
    if (!sd_card_is_mounted()) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    DIR *dir = opendir(SD_MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open %s directory", SD_MOUNT_POINT);
        return ESP_FAIL;
    }

    g_player.song_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL && g_player.song_count < MAX_SONG_FILES) {
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
                    song_info_t *song = &g_player.song_list[g_player.song_count];
                    strncpy(song->filename, entry->d_name, MAX_FILE_NAME_LEN - 1);
                    song->filename[MAX_FILE_NAME_LEN - 1] = '\0';

                    extract_display_name(entry->d_name, song->display_name, MAX_FILE_NAME_LEN);

                    /* Get file size */
                    song->file_size = sd_card_get_file_size(entry->d_name);

                    g_player.song_count++;
                }
            }
        }
    }

    closedir(dir);

    ESP_LOGI(TAG, "Loaded %d songs from Flash storage", g_player.song_count);
    return ESP_OK;
}

/* Simple audio playback task with MP3 decoding using minimp3 */
static void playback_task(void *pvParameters)
{
    FILE *fp = NULL;
    char current_file[MAX_FILE_NAME_LEN + 256] = {0};
    int current_song = -1;

    /* minimp3 decoder */
    mp3dec_t mp3d;
    mp3dec_init(&mp3d);

    /* MP3 input buffer - must be large enough for at least one complete frame */
    uint8_t *mp3_buffer = malloc(16384);
    /* PCM output buffer - max samples per frame * 2 channels (for stereo) */
    int16_t *pcm_buffer = malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * 2 * sizeof(int16_t));
    int current_sample_rate = 0;

    if (mp3_buffer == NULL || pcm_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate playback buffers");
        if (mp3_buffer) free(mp3_buffer);
        if (pcm_buffer) free(pcm_buffer);
        vTaskDelete(NULL);
        return;
    }

    size_t mp3_buffered = 0;

    ESP_LOGI(TAG, "Playback task started with minimp3 decoder");

    while (1) {
        xSemaphoreTake(g_player.state_mutex, portMAX_DELAY);
        player_state_t state = g_player.state;
        int song_idx = g_player.playback.current_song_index;
        xSemaphoreGive(g_player.state_mutex);

        if (state == STATE_PLAYING) {
            /* Check if we need to open a new file */
            if (song_idx != current_song || fp == NULL) {
                if (fp != NULL) {
                    fclose(fp);
                    fp = NULL;
                }
                mp3_buffered = 0;
                current_sample_rate = 0;
                mp3dec_init(&mp3d); /* Reinitialize decoder for new file */

                if (song_idx >= 0 && song_idx < g_player.song_count) {
                    snprintf(current_file, sizeof(current_file),
                             "%s/%s", SD_MOUNT_POINT, g_player.song_list[song_idx].filename);
                    fp = fopen(current_file, "rb");
                    if (fp) {
                        current_song = song_idx;
                        ESP_LOGI(TAG, "Opened: %s", current_file);

                        /* Read initial data */
                        mp3_buffered = fread(mp3_buffer, 1, 16384, fp);
                    } else {
                        ESP_LOGE(TAG, "Failed to open: %s", current_file);
                        g_player.state = STATE_IDLE;
                        current_song = -1;
                    }
                }
            }

            /* Decode and play audio data */
            if (fp != NULL && mp3_buffered > 0) {
                mp3dec_frame_info_t info;
                int samples = mp3dec_decode_frame(&mp3d, mp3_buffer, mp3_buffered, pcm_buffer, &info);

                if (samples > 0) {
                    /* Check if sample rate changed and update I2S */
                    if (info.hz != current_sample_rate) {
                        current_sample_rate = info.hz;
                        ESP_LOGI(TAG, "MP3 sample rate: %d Hz, channels: %d", info.hz, info.channels);
                        i2s_audio_set_sample_rate(info.hz);
                    }

                    /* Calculate bytes to send to I2S (samples * channels * 2 bytes) */
                    size_t pcm_bytes = samples * info.channels * sizeof(int16_t);

                    /* Convert mono to stereo if needed */
                    if (info.channels == 1) {
                        for (int i = samples - 1; i >= 0; i--) {
                            pcm_buffer[i * 2] = pcm_buffer[i];
                            pcm_buffer[i * 2 + 1] = pcm_buffer[i];
                        }
                        pcm_bytes = samples * 2 * sizeof(int16_t);
                    }

                    size_t bytes_written = 0;

                    /* Send decoded PCM to I2S */
                    esp_err_t ret = i2s_audio_write((const uint8_t *)pcm_buffer, pcm_bytes, &bytes_written);
                    if (ret != ESP_OK) {
                        ESP_LOGW(TAG, "I2S write error: %s", esp_err_to_name(ret));
                    }

                    /* Update position */
                    xSemaphoreTake(g_player.state_mutex, portMAX_DELAY);
                    g_player.playback.resume_position += info.frame_bytes;
                    xSemaphoreGive(g_player.state_mutex);
                }

                if (info.frame_bytes > 0) {
                    /* Move remaining data to beginning of buffer */
                    mp3_buffered -= info.frame_bytes;
                    if (mp3_buffered > 0) {
                        memmove(mp3_buffer, mp3_buffer + info.frame_bytes, mp3_buffered);
                    }

                    /* Refill buffer from file */
                    size_t to_read = 16384 - mp3_buffered;
                    if (to_read > 0) {
                        size_t bytes_read = fread(mp3_buffer + mp3_buffered, 1, to_read, fp);
                        mp3_buffered += bytes_read;
                    }
                } else {
                    /* No frame decoded, need more data or error */
                    if (mp3_buffered < 1024) {
                        /* Try to read more data */
                        size_t bytes_read = fread(mp3_buffer + mp3_buffered, 1, 16384 - mp3_buffered, fp);
                        if (bytes_read > 0) {
                            mp3_buffered += bytes_read;
                        } else {
                            /* End of file */
                            ESP_LOGI(TAG, "Song finished, moving to next");
                            fclose(fp);
                            fp = NULL;
                            current_song = -1;
                            mp3_buffered = 0;
                            current_sample_rate = 0;
                            mp3_player_send_cmd(CMD_NEXT, 0);
                        }
                    } else {
                        /* Corrupted data, skip a byte */
                        mp3_buffered--;
                        memmove(mp3_buffer, mp3_buffer + 1, mp3_buffered);
                    }
                }
            } else if (fp != NULL && mp3_buffered == 0) {
                /* End of file */
                ESP_LOGI(TAG, "Song finished, moving to next");
                fclose(fp);
                fp = NULL;
                current_song = -1;
                current_sample_rate = 0;
                mp3_player_send_cmd(CMD_NEXT, 0);
            }
        } else {
            /* Not playing - small delay to prevent busy waiting */
            vTaskDelay(pdMS_TO_TICKS(50));

            /* Close file if stopped */
            if (state == STATE_STOPPED && fp != NULL) {
                fclose(fp);
                fp = NULL;
                current_song = -1;
                mp3_buffered = 0;
                current_sample_rate = 0;
            }
        }
    }

    /* Cleanup */
    if (fp != NULL) {
        fclose(fp);
    }
    free(mp3_buffer);
    free(pcm_buffer);
    vTaskDelete(NULL);
}

void mp3_player_get_state_string(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) return;

    xSemaphoreTake(g_player.state_mutex, portMAX_DELAY);
    player_state_t state = g_player.state;
    xSemaphoreGive(g_player.state_mutex);

    switch (state) {
        case STATE_IDLE:
            strncpy(buf, "IDLE", buf_size - 1);
            break;
        case STATE_PLAYING:
            strncpy(buf, "PLAYING", buf_size - 1);
            break;
        case STATE_PAUSED:
            strncpy(buf, "PAUSED", buf_size - 1);
            break;
        case STATE_STOPPED:
            strncpy(buf, "STOPPED", buf_size - 1);
            break;
        default:
            strncpy(buf, "UNKNOWN", buf_size - 1);
    }
    buf[buf_size - 1] = '\0';
}

const char* mp3_player_get_current_song_name(void)
{
    xSemaphoreTake(g_player.state_mutex, portMAX_DELAY);
    int idx = g_player.playback.current_song_index;
    xSemaphoreGive(g_player.state_mutex);

    if (idx >= 0 && idx < g_player.song_count) {
        return g_player.song_list[idx].display_name;
    }
    return "None";
}

int mp3_player_get_current_position(void)
{
    return (int)i2s_audio_get_position();
}

int mp3_player_get_song_count(void)
{
    return g_player.song_count;
}

/* Helper functions */
bool is_mp3_file(const char *filename)
{
    size_t len = strlen(filename);
    if (len < 5) return false;

    const char *ext = filename + len - 4;
    return (strcasecmp(ext, ".mp3") == 0);
}

void extract_display_name(const char *filename, char *display_name, size_t max_len)
{
    /* Copy filename and remove .mp3 extension */
    strncpy(display_name, filename, max_len - 1);
    display_name[max_len - 1] = '\0';

    size_t len = strlen(display_name);
    if (len > 4) {
        char *ext = display_name + len - 4;
        if (strcasecmp(ext, ".mp3") == 0) {
            *ext = '\0';
        }
    }
}
