/**
 * @file mp3_player.h
 * @brief MP3 Player main header file
 */

#ifndef MP3_PLAYER_H
#define MP3_PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration constants */
#define MP3_TAG                     "MP3_PLAYER"
#define NVS_NAMESPACE               "mp3_config"
#define NVS_KEY_PLAYER_NAME         "player_name"
#define NVS_KEY_SONG_INDEX          "song_index"
#define NVS_KEY_PLAYBACK_POS        "playback_pos"
#define MAX_PLAYER_NAME_LEN         32
#define MAX_SONG_FILES              100
#define MAX_FILE_NAME_LEN           64
#define SONG_LIST_FILE              "/sdcard/songs.txt"
#define STORAGE_MOUNT_POINT         "/sdcard"

/* Command types for player control */
typedef enum {
    CMD_NONE = 0,
    CMD_PLAY,
    CMD_PAUSE,
    CMD_RESUME,
    CMD_NEXT,
    CMD_PREV,
    CMD_STOP,
    CMD_SET_VOLUME,
    CMD_SET_SONG,
} player_cmd_t;

/* Player state */
typedef enum {
    STATE_IDLE = 0,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_STOPPED,
} player_state_t;

/* Audio format info */
typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
} audio_format_t;

/* Song info structure */
typedef struct {
    char filename[MAX_FILE_NAME_LEN];
    char display_name[MAX_FILE_NAME_LEN];
    uint32_t duration_ms;
    uint32_t file_size;
} song_info_t;

/* Playback context */
typedef struct {
    uint32_t current_position;
    uint32_t resume_position;
    int current_song_index;
    bool has_resume_point;
} playback_context_t;

/* Player configuration */
typedef struct {
    char player_name[MAX_PLAYER_NAME_LEN];
    bool is_first_boot;
    int volume;
} player_config_t;

/* Global player state */
typedef struct {
    player_state_t state;
    player_config_t config;
    playback_context_t playback;
    song_info_t song_list[MAX_SONG_FILES];
    int song_count;
    QueueHandle_t cmd_queue;
    SemaphoreHandle_t state_mutex;
} mp3_player_t;

/* Global player instance */
extern mp3_player_t g_player;

/* Main player functions */
esp_err_t mp3_player_init(void);
esp_err_t mp3_player_start(void);
esp_err_t mp3_player_send_cmd(player_cmd_t cmd, int param);
void mp3_player_get_state_string(char *buf, size_t buf_size);
const char* mp3_player_get_current_song_name(void);
int mp3_player_get_current_position(void);
int mp3_player_get_song_count(void);

/* Configuration functions */
esp_err_t config_load(player_config_t *config);
esp_err_t config_save(const player_config_t *config);
esp_err_t config_set_player_name(const char *name);
esp_err_t config_save_playback_state(int song_index, uint32_t position);
esp_err_t config_load_playback_state(int *song_index, uint32_t *position);

/* Song list functions */
esp_err_t song_list_load(void);
esp_err_t song_list_scan_sdcard(void);
esp_err_t song_list_save_to_flash(void);
const song_info_t* song_list_get(int index);
void song_list_print(void);

/* Helper functions */
bool is_mp3_file(const char *filename);
void extract_display_name(const char *filename, char *display_name, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* MP3_PLAYER_H */
