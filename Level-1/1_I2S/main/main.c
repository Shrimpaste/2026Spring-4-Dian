/**
 * @file main.c
 * @brief Level-1 I2S 数字音频合成实验 - 模块化版本
 *
 * 本实验通过模块化封装实现了以下功能：
 * - audio_generator: 音频波形生成库
 * - i2s_driver: I2S驱动封装库
 * - button_driver: 按键检测库
 * - task_manager: 实验任务管理库
 *
 * 实验要求:
 * 1. 右声道输出500Hz锯齿波
 * 2. 左声道1001Hz正弦波 + 右声道999Hz正弦波(L+R模式)，可听见拍频
 * 3. 左声道1kHz正弦波 + 右声道4kHz正弦波，通过BOOT按键切换声道模式
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio_generator.h"
#include "i2s_driver.h"
#include "button_driver.h"
#include "task_manager.h"

static const char *TAG = "MAIN";

/**
 * @brief 实验主入口
 */
void app_main(void)
{
    ESP_LOGI(TAG, "================================");
    ESP_LOGI(TAG, "Level-1 I2S 数字音频合成实验");
    ESP_LOGI(TAG, "模块化版本");
    ESP_LOGI(TAG, "================================");

    // ==================== 初始化各模块 ====================

    // 1. 初始化音频生成器
    audio_gen_config_t audio_cfg = {
        .sample_rate = 44100,
        .amplitude = 8000,
    };
    audio_generator_handle_t audio_gen = audio_gen_create(&audio_cfg);
    if (audio_gen == NULL) {
        ESP_LOGE(TAG, "音频生成器初始化失败");
        return;
    }
    ESP_LOGI(TAG, "音频生成器初始化完成");

    // 2. 初始化I2S驱动
    i2s_driver_config_t i2s_cfg = {
        .sample_rate = 44100,
        .bits_per_sample = 16,
        .gpio_ws = 5,
        .gpio_bclk = 6,
        .gpio_dout = 7,
        .stereo = true,
    };
    i2s_driver_handle_t i2s_drv = i2s_driver_init(&i2s_cfg);
    if (i2s_drv == NULL) {
        ESP_LOGE(TAG, "I2S驱动初始化失败");
        audio_gen_destroy(audio_gen);
        return;
    }

    // 3. 初始化按键驱动
    button_config_t btn_cfg = {
        .gpio_num = GPIO_NUM_0,
        .active_low = true,
        .debounce_ms = 50,
        .long_press_ms = 3000,
        .internal_pullup = true,
    };
    button_handle_t btn = button_init(&btn_cfg);
    if (btn == NULL) {
        ESP_LOGE(TAG, "按键驱动初始化失败");
        i2s_driver_deinit(i2s_drv);
        audio_gen_destroy(audio_gen);
        return;
    }
    ESP_LOGI(TAG, "按键驱动初始化完成 (GPIO%d)", btn_cfg.gpio_num);

    // 4. 创建任务管理器
    task_manager_config_t tm_cfg = {
        .sample_count = 512,
        .task3_long_press_ms = 3000,
    };
    task_manager_handle_t task_mgr = task_manager_create(i2s_drv, audio_gen, btn, &tm_cfg);
    if (task_mgr == NULL) {
        ESP_LOGE(TAG, "任务管理器创建失败");
        button_deinit(btn);
        i2s_driver_deinit(i2s_drv);
        audio_gen_destroy(audio_gen);
        return;
    }
    ESP_LOGI(TAG, "任务管理器创建完成");

    // 3秒倒计时
    ESP_LOGI(TAG, "3秒后开始任务1...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // ==================== 主循环 ====================

    ESP_LOGI(TAG, "开始主循环");

    while (1) {
        // 执行任务管理器循环(非阻塞)
        task_manager_run(task_mgr);

        // 延时以减少CPU占用
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}