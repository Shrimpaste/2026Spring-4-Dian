/**
 * @file task_manager.c
 * @brief 实验任务管理库实现
 */

#include "task_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "TASK_MGR";

/**
 * @brief 任务管理器实例结构体
 */
typedef struct task_manager_s {
    i2s_driver_handle_t i2s;            /**< I2S驱动 */
    audio_generator_handle_t audio;     /**< 音频生成器 */
    button_handle_t button;             /**< 按键 */
    task_manager_config_t config;       /**< 配置 */

    task_id_t current_task;             /**< 当前任务 */
    channel_submode_t channel_mode;     /**< 任务3声道模式 */
    bool task_switched;                 /**< 任务刚切换标志 */

    int16_t *buffer;                    /**< 音频缓冲区 */
} task_manager_t;

task_manager_handle_t task_manager_create(i2s_driver_handle_t i2s,
                                           audio_generator_handle_t audio,
                                           button_handle_t btn,
                                           const task_manager_config_t *config)
{
    if (i2s == NULL || audio == NULL || btn == NULL) {
        ESP_LOGE(TAG, "无效参数");
        return NULL;
    }

    task_manager_t *tm = (task_manager_t *)malloc(sizeof(task_manager_t));
    if (tm == NULL) {
        return NULL;
    }

    tm->i2s = i2s;
    tm->audio = audio;
    tm->button = btn;
    tm->current_task = TASK_1_SAWTOOTH;
    tm->channel_mode = CH_MODE_LEFT;
    tm->task_switched = true;

    if (config != NULL) {
        memcpy(&tm->config, config, sizeof(task_manager_config_t));
    } else {
        tm->config = (task_manager_config_t)TASK_MANAGER_DEFAULT_CONFIG();
    }

    // 分配音频缓冲区
    tm->buffer = (int16_t *)malloc(tm->config.sample_count * sizeof(int16_t) * 2);
    if (tm->buffer == NULL) {
        ESP_LOGE(TAG, "缓冲区内存分配失败");
        free(tm);
        return NULL;
    }

    return (task_manager_handle_t)tm;
}

void task_manager_destroy(task_manager_handle_t tm)
{
    if (tm == NULL) {
        return;
    }

    task_manager_t *mgr = (task_manager_t *)tm;

    if (mgr->buffer != NULL) {
        free(mgr->buffer);
    }

    free(mgr);
}

task_id_t task_manager_get_current_task(task_manager_handle_t tm)
{
    if (tm == NULL) {
        return TASK_1_SAWTOOTH;
    }
    return ((task_manager_t *)tm)->current_task;
}

void task_manager_set_task(task_manager_handle_t tm, task_id_t task)
{
    if (tm == NULL || task < TASK_1_SAWTOOTH || task > TASK_3_CHANNEL_SWITCH) {
        return;
    }

    task_manager_t *mgr = (task_manager_t *)tm;
    if (mgr->current_task != task) {
        mgr->current_task = task;
        mgr->task_switched = true;

        // 任务切换时重置声道模式
        if (task == TASK_3_CHANNEL_SWITCH) {
            mgr->channel_mode = CH_MODE_LEFT;
        }
    }
}

void task_manager_next_task(task_manager_handle_t tm)
{
    if (tm == NULL) {
        return;
    }

    task_manager_t *mgr = (task_manager_t *)tm;
    task_id_t next = (mgr->current_task % 3) + 1;
    task_manager_set_task(tm, next);
}

channel_submode_t task_manager_get_channel_mode(task_manager_handle_t tm)
{
    if (tm == NULL) {
        return CH_MODE_LEFT;
    }
    return ((task_manager_t *)tm)->channel_mode;
}

void task_manager_set_channel_mode(task_manager_handle_t tm, channel_submode_t mode)
{
    if (tm == NULL || mode < CH_MODE_LEFT || mode > CH_MODE_LR_MIX) {
        return;
    }

    task_manager_t *mgr = (task_manager_t *)tm;
    if (mgr->channel_mode != mode) {
        mgr->channel_mode = mode;
        ESP_LOGI(TAG, "声道模式切换为: %s", task_manager_get_channel_mode_name(mode));
    }
}

/**
 * @brief 任务1: 右声道500Hz锯齿波
 */
static void task1_sawtooth(task_manager_t *tm)
{
    if (tm->task_switched) {
        ESP_LOGI(TAG, "【任务1】右声道 500Hz 锯齿波");
        ESP_LOGI(TAG, "短按BOOT键切换到任务2");
        tm->task_switched = false;
    }

    // 生成右声道500Hz锯齿波
    audio_gen_sawtooth_wave(tm->audio, tm->buffer, tm->config.sample_count,
                            500, CHANNEL_RIGHT_ONLY);

    // 发送音频
    i2s_driver_write(tm->i2s, tm->buffer, tm->config.sample_count, -1);

    // 检测按键
    button_event_t event = button_update(tm->button);
    if (event == BUTTON_EVENT_SHORT_PRESS) {
        task_manager_next_task((task_manager_handle_t)tm);
    }
}

/**
 * @brief 任务2: 拍频效果
 */
static void task2_beat_frequency(task_manager_t *tm)
{
    if (tm->task_switched) {
        ESP_LOGI(TAG, "【任务2】拍频效果");
        ESP_LOGI(TAG, "左声道: 1001Hz 正弦波");
        ESP_LOGI(TAG, "右声道: 999Hz 正弦波");
        ESP_LOGI(TAG, "拍频: 2Hz (每秒强弱变化2次)");
        ESP_LOGI(TAG, "短按BOOT键切换到任务3");
        tm->task_switched = false;
    }

    // 生成双声道不同频率正弦波
    audio_gen_sine_wave(tm->audio, tm->buffer, tm->config.sample_count,
                        1001, 999, CHANNEL_INDEPENDENT);

    // 发送音频
    i2s_driver_write(tm->i2s, tm->buffer, tm->config.sample_count, -1);

    // 检测按键
    button_event_t event = button_update(tm->button);
    if (event == BUTTON_EVENT_SHORT_PRESS) {
        task_manager_next_task((task_manager_handle_t)tm);
    }
}

/**
 * @brief 任务3: 按键切换声道
 */
static void task3_channel_switch(task_manager_t *tm)
{
    static channel_submode_t last_mode = 0xFF;

    if (tm->task_switched) {
        ESP_LOGI(TAG, "【任务3】按键切换声道模式");
        ESP_LOGI(TAG, "左声道: 1000Hz 正弦波");
        ESP_LOGI(TAG, "右声道: 4000Hz 正弦波");
        ESP_LOGI(TAG, "短按BOOT: 切换声道 (左→右→L+R)");
        ESP_LOGI(TAG, "长按%ds: 返回任务1", tm->config.task3_long_press_ms / 1000);
        tm->task_switched = false;
        last_mode = 0xFF;
    }

    // 模式变化时打印信息
    if (tm->channel_mode != last_mode) {
        ESP_LOGI(TAG, "当前声道: %s", task_manager_get_channel_mode_name(tm->channel_mode));
        last_mode = tm->channel_mode;
    }

    // 根据声道模式生成音频
    switch (tm->channel_mode) {
        case CH_MODE_LEFT:
            // 仅左声道 1kHz
            audio_gen_sine_wave(tm->audio, tm->buffer, tm->config.sample_count,
                                1000, 0, CHANNEL_LEFT_ONLY);
            break;

        case CH_MODE_RIGHT:
            // 仅右声道 4kHz
            audio_gen_sine_wave(tm->audio, tm->buffer, tm->config.sample_count,
                                4000, 0, CHANNEL_RIGHT_ONLY);
            break;

        case CH_MODE_LR_MIX:
            // L+R: 左1kHz + 右4kHz
            audio_gen_sine_wave(tm->audio, tm->buffer, tm->config.sample_count,
                                1000, 4000, CHANNEL_INDEPENDENT);
            break;

        default:
            break;
    }

    // 发送音频
    i2s_driver_write(tm->i2s, tm->buffer, tm->config.sample_count, -1);

    // 检测按键
    button_event_t event = button_update(tm->button);

    if (event == BUTTON_EVENT_SHORT_PRESS) {
        // 短按切换声道模式
        channel_submode_t next = (tm->channel_mode + 1) % 3;
        task_manager_set_channel_mode((task_manager_handle_t)tm, next);
    } else if (event == BUTTON_EVENT_LONG_PRESS) {
        // 长按返回任务1
        ESP_LOGI(TAG, "长按检测，返回任务1");
        task_manager_set_task((task_manager_handle_t)tm, TASK_1_SAWTOOTH);
    }
}

bool task_manager_run(task_manager_handle_t tm)
{
    if (tm == NULL) {
        return false;
    }

    task_manager_t *mgr = (task_manager_t *)tm;

    switch (mgr->current_task) {
        case TASK_1_SAWTOOTH:
            task1_sawtooth(mgr);
            break;

        case TASK_2_BEAT_FREQ:
            task2_beat_frequency(mgr);
            break;

        case TASK_3_CHANNEL_SWITCH:
            task3_channel_switch(mgr);
            break;

        default:
            mgr->current_task = TASK_1_SAWTOOTH;
            mgr->task_switched = true;
            break;
    }

    return true;
}

void task_manager_execute_task(task_manager_handle_t tm, task_id_t task)
{
    if (tm == NULL) {
        return;
    }

    task_manager_set_task(tm, task);

    // 循环执行直到任务切换
    task_manager_t *mgr = (task_manager_t *)tm;
    task_id_t original_task = task;

    while (mgr->current_task == original_task) {
        task_manager_run(tm);
    }
}

const char* task_manager_get_task_name(task_id_t task)
{
    switch (task) {
        case TASK_1_SAWTOOTH:
            return "任务1: 500Hz锯齿波";
        case TASK_2_BEAT_FREQ:
            return "任务2: 拍频效果";
        case TASK_3_CHANNEL_SWITCH:
            return "任务3: 声道切换";
        default:
            return "未知任务";
    }
}

const char* task_manager_get_channel_mode_name(channel_submode_t mode)
{
    switch (mode) {
        case CH_MODE_LEFT:
            return "左声道";
        case CH_MODE_RIGHT:
            return "右声道";
        case CH_MODE_LR_MIX:
            return "L+R混合";
        default:
            return "未知模式";
    }
}