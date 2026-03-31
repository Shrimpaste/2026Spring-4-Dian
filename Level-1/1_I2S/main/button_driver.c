/**
 * @file button_driver.c
 * @brief 按键驱动库实现
 */

#include "button_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 按键实例结构体
 */
typedef struct button_s {
    button_config_t config;     /**< 配置参数 */
    uint32_t press_start_tick;  /**< 按下开始时间 */
    bool is_pressed;            /**< 当前是否按下 */
    bool debounce_active;       /**< 消抖中 */
    uint32_t debounce_start;    /**< 消抖开始时间 */
    bool long_press_reported;   /**< 已报告长按 */
} button_t;

button_handle_t button_init(const button_config_t *config)
{
    button_t *btn = (button_t *)malloc(sizeof(button_t));
    if (btn == NULL) {
        return NULL;
    }

    // 使用默认配置或用户配置
    if (config != NULL) {
        memcpy(&btn->config, config, sizeof(button_config_t));
    } else {
        btn->config = (button_config_t)BUTTON_DEFAULT_CONFIG();
    }

    // 初始化状态
    btn->press_start_tick = 0;
    btn->is_pressed = false;
    btn->debounce_active = false;
    btn->debounce_start = 0;
    btn->long_press_reported = false;

    // 配置GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << btn->config.gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = btn->config.internal_pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    return (button_handle_t)btn;
}

void button_deinit(button_handle_t btn)
{
    if (btn != NULL) {
        free(btn);
    }
}

/**
 * @brief 获取当前GPIO电平
 */
static bool get_raw_level(button_t *btn)
{
    int level = gpio_get_level(btn->config.gpio_num);
    if (btn->config.active_low) {
        return level == 0;  // 低电平有效
    } else {
        return level == 1;  // 高电平有效
    }
}

button_event_t button_update(button_handle_t btn)
{
    if (btn == NULL) {
        return BUTTON_EVENT_NONE;
    }

    button_t *b = (button_t *)btn;
    uint32_t now = xTaskGetTickCount();
    bool raw_pressed = get_raw_level(b);

    // 消抖处理
    if (b->debounce_active) {
        if ((now - b->debounce_start) >= pdMS_TO_TICKS(b->config.debounce_ms)) {
            b->debounce_active = false;
        } else {
            return BUTTON_EVENT_NONE;  // 仍在消抖中
        }
    }

    // 状态变化检测
    if (raw_pressed && !b->is_pressed) {
        // 刚按下
        b->is_pressed = true;
        b->press_start_tick = now;
        b->debounce_active = true;
        b->debounce_start = now;
        b->long_press_reported = false;
        return BUTTON_EVENT_PRESSED;
    }

    if (!raw_pressed && b->is_pressed) {
        // 刚释放
        b->is_pressed = false;
        b->debounce_active = true;
        b->debounce_start = now;

        uint32_t duration = now - b->press_start_tick;
        if (duration >= pdMS_TO_TICKS(b->config.long_press_ms)) {
            return BUTTON_EVENT_LONG_PRESS;
        } else {
            return BUTTON_EVENT_SHORT_PRESS;
        }
    }

    // 持续按下中 - 检测长按(但只报告一次)
    if (b->is_pressed && !b->long_press_reported) {
        uint32_t duration = now - b->press_start_tick;
        if (duration >= pdMS_TO_TICKS(b->config.long_press_ms)) {
            b->long_press_reported = true;
            return BUTTON_EVENT_LONG_PRESS;
        }
    }

    return BUTTON_EVENT_NONE;
}

bool button_is_pressed(button_handle_t btn)
{
    if (btn == NULL) {
        return false;
    }

    button_t *b = (button_t *)btn;

    // 先检查当前状态
    if (!get_raw_level(b)) {
        return false;
    }

    // 消抖延时
    vTaskDelay(pdMS_TO_TICKS(b->config.debounce_ms));

    // 再次检查
    if (!get_raw_level(b)) {
        return false;
    }

    return true;
}

void button_wait_release(button_handle_t btn)
{
    if (btn == NULL) {
        return;
    }

    button_t *b = (button_t *)btn;

    while (get_raw_level(b)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 释放后消抖
    vTaskDelay(pdMS_TO_TICKS(b->config.debounce_ms));
}

uint32_t button_get_press_duration(button_handle_t btn)
{
    if (btn == NULL) {
        return 0;
    }

    button_t *b = (button_t *)btn;

    if (!b->is_pressed) {
        return 0;
    }

    uint32_t duration_ticks = xTaskGetTickCount() - b->press_start_tick;
    return duration_ticks * portTICK_PERIOD_MS;
}

void button_reset(button_handle_t btn)
{
    if (btn == NULL) {
        return;
    }

    button_t *b = (button_t *)btn;
    b->is_pressed = false;
    b->debounce_active = false;
    b->long_press_reported = false;
    b->press_start_tick = 0;
}

void button_set_long_press_time(button_handle_t btn, uint32_t long_press_ms)
{
    if (btn != NULL) {
        ((button_t *)btn)->config.long_press_ms = long_press_ms;
    }
}