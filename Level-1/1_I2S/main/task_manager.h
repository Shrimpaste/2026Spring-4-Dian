/**
 * @file task_manager.h
 * @brief 实验任务管理库
 *
 * 管理I2S实验的三个任务：
 * - 任务1: 右声道500Hz锯齿波
 * - 任务2: 拍频效果(左1001Hz + 右999Hz)
 * - 任务3: 按键切换声道(左1kHz + 右4kHz)
 */

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "audio_generator.h"
#include "i2s_driver.h"
#include "button_driver.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 任务ID枚举
 */
typedef enum {
    TASK_1_SAWTOOTH = 1,        /**< 任务1: 锯齿波 */
    TASK_2_BEAT_FREQ = 2,       /**< 任务2: 拍频 */
    TASK_3_CHANNEL_SWITCH = 3,  /**< 任务3: 声道切换 */
} task_id_t;

/**
 * @brief 声道子模式(任务3专用)
 */
typedef enum {
    CH_MODE_LEFT = 0,           /**< 仅左声道 */
    CH_MODE_RIGHT = 1,          /**< 仅右声道 */
    CH_MODE_LR_MIX = 2,         /**< L+R混合 */
} channel_submode_t;

/**
 * @brief 任务管理器配置
 */
typedef struct {
    int sample_count;           /**< 每次生成样本数 */
    int task3_long_press_ms;    /**< 任务3返回任务1的长按时间(ms) */
} task_manager_config_t;

/**
 * @brief 任务管理器句柄
 */
typedef struct task_manager_s* task_manager_handle_t;

/**
 * @brief 默认任务管理器配置
 */
#define TASK_MANAGER_DEFAULT_CONFIG() { \
    .sample_count = 512, \
    .task3_long_press_ms = 3000, \
}

/**
 * @brief 创建任务管理器
 * @param i2s I2S驱动句柄
 * @param audio 音频生成器句柄
 * @param btn 按键句柄
 * @param config 配置参数，传NULL使用默认配置
 * @return 管理器句柄，失败返回NULL
 */
task_manager_handle_t task_manager_create(i2s_driver_handle_t i2s,
                                           audio_generator_handle_t audio,
                                           button_handle_t btn,
                                           const task_manager_config_t *config);

/**
 * @brief 销毁任务管理器
 * @param tm 管理器句柄
 */
void task_manager_destroy(task_manager_handle_t tm);

/**
 * @brief 获取当前任务ID
 * @param tm 管理器句柄
 * @return 当前任务ID
 */
task_id_t task_manager_get_current_task(task_manager_handle_t tm);

/**
 * @brief 设置当前任务
 * @param tm 管理器句柄
 * @param task 任务ID
 */
void task_manager_set_task(task_manager_handle_t tm, task_id_t task);

/**
 * @brief 切换到下一个任务
 * @param tm 管理器句柄
 */
void task_manager_next_task(task_manager_handle_t tm);

/**
 * @brief 获取任务3的当前声道模式
 * @param tm 管理器句柄
 * @return 声道子模式
 */
channel_submode_t task_manager_get_channel_mode(task_manager_handle_t tm);

/**
 * @brief 设置任务3的声道模式
 * @param tm 管理器句柄
 * @param mode 声道子模式
 */
void task_manager_set_channel_mode(task_manager_handle_t tm, channel_submode_t mode);

/**
 * @brief 执行任务循环(非阻塞，需在主循环中调用)
 * @param tm 管理器句柄
 * @return true=正常执行, false=错误
 */
bool task_manager_run(task_manager_handle_t tm);

/**
 * @brief 执行特定任务(阻塞式，直到任务切换)
 * @param tm 管理器句柄
 * @param task 任务ID
 */
void task_manager_execute_task(task_manager_handle_t tm, task_id_t task);

/**
 * @brief 获取任务名称
 * @param task 任务ID
 * @return 任务名称字符串
 */
const char* task_manager_get_task_name(task_id_t task);

/**
 * @brief 获取声道模式名称
 * @param mode 声道子模式
 * @return 模式名称字符串
 */
const char* task_manager_get_channel_mode_name(channel_submode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* TASK_MANAGER_H */