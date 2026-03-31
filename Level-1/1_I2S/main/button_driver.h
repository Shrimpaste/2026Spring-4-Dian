/**
 * @file button_driver.h
 * @brief 按键驱动库
 *
 * 支持GPIO按键的初始化、消抖检测、短按/长按识别
 * 非阻塞设计，可在主循环中轮询检测
 */

#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 按键事件类型
 */
typedef enum {
    BUTTON_EVENT_NONE = 0,      /**< 无事件 */
    BUTTON_EVENT_PRESSED,       /**< 按下(未释放) */
    BUTTON_EVENT_SHORT_PRESS,   /**< 短按释放 */
    BUTTON_EVENT_LONG_PRESS,    /**< 长按释放 */
} button_event_t;

/**
 * @brief 按键配置结构体
 */
typedef struct {
    gpio_num_t gpio_num;        /**< GPIO引脚号 */
    bool active_low;            /**< 低电平有效(通常为true) */
    uint32_t debounce_ms;       /**< 消抖时间(ms) */
    uint32_t long_press_ms;     /**< 长按阈值(ms) */
    bool internal_pullup;       /**< 使能内部上拉 */
} button_config_t;

/**
 * @brief 按键句柄
 */
typedef struct button_s* button_handle_t;

/**
 * @brief 默认按键配置(GPIO0, 低电平有效, 50ms消抖, 1000ms长按)
 */
#define BUTTON_DEFAULT_CONFIG() { \
    .gpio_num = GPIO_NUM_0, \
    .active_low = true, \
    .debounce_ms = 50, \
    .long_press_ms = 1000, \
    .internal_pullup = true, \
}

/**
 * @brief 初始化按键
 * @param config 配置参数，传NULL使用默认配置
 * @return 按键句柄，失败返回NULL
 */
button_handle_t button_init(const button_config_t *config);

/**
 * @brief 反初始化按键
 * @param btn 按键句柄
 */
void button_deinit(button_handle_t btn);

/**
 * @brief 更新按键状态(需在主循环中定期调用)
 * @param btn 按键句柄
 * @return 按键事件
 */
button_event_t button_update(button_handle_t btn);

/**
 * @brief 检查按键是否按下(阻塞式，带消抖)
 * @param btn 按键句柄
 * @return true=按下，false=未按下
 */
bool button_is_pressed(button_handle_t btn);

/**
 * @brief 等待按键释放(阻塞式)
 * @param btn 按键句柄
 */
void button_wait_release(button_handle_t btn);

/**
 * @brief 获取按键按下时长(ms)
 * @param btn 按键句柄
 * @return 按下时长，未按下返回0
 */
uint32_t button_get_press_duration(button_handle_t btn);

/**
 * @brief 重置按键状态
 * @param btn 按键句柄
 */
void button_reset(button_handle_t btn);

/**
 * @brief 设置长按阈值
 * @param btn 按键句柄
 * @param long_press_ms 长按阈值(ms)
 */
void button_set_long_press_time(button_handle_t btn, uint32_t long_press_ms);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_DRIVER_H */