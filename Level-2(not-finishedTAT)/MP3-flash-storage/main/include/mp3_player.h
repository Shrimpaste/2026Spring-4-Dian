/**
 * @file mp3_player.h
 * @brief MP3播放器模块
 *
 * 实现基于SPIFFS文件系统的MP3播放功能
 * 支持播放列表管理、播放/暂停/切换等功能
 */

#ifndef MP3_PLAYER_H
#define MP3_PLAYER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "i2s_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MP3_PLAYER_MAX_FILES    20      /** < 最大支持的MP3文件数量 */
#define MP3_PLAYER_FILE_NAME_MAX 64      /** < 文件名最大长度 */

/**
 * @brief 播放器状态
 */
typedef enum {
    MP3_STATE_STOPPED = 0,   /** < 停止状态 */
    MP3_STATE_PLAYING,       /** < 播放中 */
    MP3_STATE_PAUSED,        /** < 暂停状态 */
} mp3_player_state_t;

/**
 * @brief 播放器事件类型
 */
typedef enum {
    MP3_EVENT_NONE = 0,
    MP3_EVENT_TRACK_STARTED,     /** < 曲目开始播放 */
    MP3_EVENT_TRACK_FINISHED,    /** < 曲目播放完成 */
    MP3_EVENT_ERROR,             /** < 播放错误 */
} mp3_player_event_t;

/**
 * @brief 播放器事件回调函数类型
 */
typedef void (*mp3_player_callback_t)(mp3_player_event_t event, int track_index, void *user_data);

/**
 * @brief MP3播放器配置
 */
typedef struct {
    i2s_driver_handle_t i2s_handle;      /** < I2S驱动句柄（可选，可在初始化时创建） */
    uint32_t            sample_rate;     /** < 采样率，默认44100 */
    uint8_t             volume;          /** < 音量 0-100，默认80 */
    mp3_player_callback_t callback;      /** < 事件回调函数 */
    void                *user_data;      /** < 用户数据 */
} mp3_player_config_t;

/**
 * @brief MP3播放器句柄
 */
typedef struct mp3_player_s* mp3_player_handle_t;

/**
 * @brief 默认配置
 */
#define MP3_PLAYER_DEFAULT_CONFIG() { \
    .i2s_handle = NULL, \
    .sample_rate = 44100, \
    .volume = 80, \
    .callback = NULL, \
    .user_data = NULL, \
}

/**
 * @brief 初始化MP3播放器和SPIFFS文件系统
 * @param config 播放器配置
 * @return 播放器句柄，失败返回NULL
 */
mp3_player_handle_t mp3_player_init(const mp3_player_config_t *config);

/**
 * @brief 反初始化MP3播放器
 * @param handle 播放器句柄
 */
void mp3_player_deinit(mp3_player_handle_t handle);

/**
 * @brief 扫描SPIFFS中的MP3文件
 * @param handle 播放器句柄
 * @return 扫描到的文件数量
 */
int mp3_player_scan_files(mp3_player_handle_t handle);

/**
 * @brief 获取文件数量
 * @param handle 播放器句柄
 * @return 文件数量
 */
int mp3_player_get_file_count(mp3_player_handle_t handle);

/**
 * @brief 获取文件名
 * @param handle 播放器句柄
 * @param index 文件索引
 * @return 文件名指针，失败返回NULL
 */
const char* mp3_player_get_file_name(mp3_player_handle_t handle, int index);

/**
 * @brief 播放指定索引的曲目
 * @param handle 播放器句柄
 * @param index 曲目索引
 * @return true=成功, false=失败
 */
bool mp3_player_play(mp3_player_handle_t handle, int index);

/**
 * @brief 暂停播放
 * @param handle 播放器句柄
 * @return true=成功, false=失败
 */
bool mp3_player_pause(mp3_player_handle_t handle);

/**
 * @brief 恢复播放
 * @param handle 播放器句柄
 * @return true=成功, false=失败
 */
bool mp3_player_resume(mp3_player_handle_t handle);

/**
 * @brief 停止播放
 * @param handle 播放器句柄
 * @return true=成功, false=失败
 */
bool mp3_player_stop(mp3_player_handle_t handle);

/**
 * @brief 播放下一首
 * @param handle 播放器句柄
 * @return true=成功, false=失败
 */
bool mp3_player_next(mp3_player_handle_t handle);

/**
 * @brief 播放上一首
 * @param handle 播放器句柄
 * @return true=成功, false=失败
 */
bool mp3_player_prev(mp3_player_handle_t handle);

/**
 * @brief 获取当前播放状态
 * @param handle 播放器句柄
 * @return 播放器状态
 */
mp3_player_state_t mp3_player_get_state(mp3_player_handle_t handle);

/**
 * @brief 获取当前播放索引
 * @param handle 播放器句柄
 * @return 当前曲目索引，未播放返回-1
 */
int mp3_player_get_current_index(mp3_player_handle_t handle);

/**
 * @brief 设置音量
 * @param handle 播放器句柄
 * @param volume 音量值 0-100
 */
void mp3_player_set_volume(mp3_player_handle_t handle, uint8_t volume);

/**
 * @brief 获取音量
 * @param handle 播放器句柄
 * @return 当前音量 0-100
 */
uint8_t mp3_player_get_volume(mp3_player_handle_t handle);

/**
 * @brief 获取I2S驱动句柄
 * @param handle 播放器句柄
 * @return I2S驱动句柄
 */
i2s_driver_handle_t mp3_player_get_i2s_handle(mp3_player_handle_t handle);

/**
 * @brief 播放器主循环（需在任务中调用）
 * @param handle 播放器句柄
 * @return true=正在播放, false=空闲或错误
 */
bool mp3_player_loop(mp3_player_handle_t handle);

/**
 * @brief 创建播放器任务并开始自动播放
 * @param handle 播放器句柄
 * @return true=成功, false=失败
 */
bool mp3_player_start_task(mp3_player_handle_t handle);

/**
 * @brief 停止播放器任务
 * @param handle 播放器句柄
 */
void mp3_player_stop_task(mp3_player_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* MP3_PLAYER_H */
