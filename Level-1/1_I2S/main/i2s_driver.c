/**
 * @file i2s_driver.c
 * @brief I2S音频驱动库实现
 */

#include "i2s_driver.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "I2S_DRV";

/**
 * @brief I2S驱动实例结构体
 */
typedef struct i2s_driver_s {
    i2s_driver_config_t config;     /**< 配置参数 */
    i2s_chan_handle_t   tx_channel; /**< I2S发送通道 */
} i2s_driver_t;

i2s_driver_handle_t i2s_driver_init(const i2s_driver_config_t *config)
{
    i2s_driver_t *drv = (i2s_driver_t *)malloc(sizeof(i2s_driver_t));
    if (drv == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return NULL;
    }

    // 使用默认配置或用户配置
    if (config != NULL) {
        memcpy(&drv->config, config, sizeof(i2s_driver_config_t));
    } else {
        drv->config = (i2s_driver_config_t)I2S_DRIVER_DEFAULT_CONFIG();
    }

    // 创建通道配置
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    // 分配TX通道
    esp_err_t ret = i2s_new_channel(&chan_cfg, &drv->tx_channel, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建I2S通道失败: %d", ret);
        free(drv);
        return NULL;
    }

    // 标准I2S配置
    i2s_slot_mode_t slot_mode = drv->config.stereo ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO;

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(drv->config.sample_rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(drv->config.bits_per_sample, slot_mode),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = drv->config.gpio_bclk,
            .ws = drv->config.gpio_ws,
            .dout = drv->config.gpio_dout,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    // 初始化标准模式
    ret = i2s_channel_init_std_mode(drv->tx_channel, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化I2S模式失败: %d", ret);
        i2s_del_channel(drv->tx_channel);
        free(drv);
        return NULL;
    }

    // 启用通道
    ret = i2s_channel_enable(drv->tx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用I2S通道失败: %d", ret);
        i2s_del_channel(drv->tx_channel);
        free(drv);
        return NULL;
    }

    ESP_LOGI(TAG, "I2S初始化完成");
    ESP_LOGI(TAG, "  采样率: %d Hz", drv->config.sample_rate);
    ESP_LOGI(TAG, "  位深: %d bit", drv->config.bits_per_sample);
    ESP_LOGI(TAG, "  声道: %s", drv->config.stereo ? "立体声" : "单声道");
    ESP_LOGI(TAG, "  GPIO: WS=%d, BCLK=%d, DOUT=%d",
             drv->config.gpio_ws, drv->config.gpio_bclk, drv->config.gpio_dout);

    return (i2s_driver_handle_t)drv;
}

bool i2s_driver_deinit(i2s_driver_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    i2s_driver_t *drv = (i2s_driver_t *)handle;

    // 禁用通道
    i2s_channel_disable(drv->tx_channel);

    // 删除通道
    i2s_del_channel(drv->tx_channel);

    free(drv);

    ESP_LOGI(TAG, "I2S反初始化完成");
    return true;
}

int i2s_driver_write(i2s_driver_handle_t handle,
                     const int16_t *buffer,
                     int sample_count,
                     int timeout_ms)
{
    if (handle == NULL || buffer == NULL || sample_count <= 0) {
        return -1;
    }

    i2s_driver_t *drv = (i2s_driver_t *)handle;

    size_t bytes_written = 0;
    size_t buffer_size = sample_count * sizeof(int16_t) * (drv->config.stereo ? 2 : 1);

    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    esp_err_t ret = i2s_channel_write(drv->tx_channel, buffer, buffer_size, &bytes_written, ticks);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "写入数据失败: %d", ret);
        return -1;
    }

    // 返回实际发送的样本帧数
    return bytes_written / sizeof(int16_t) / (drv->config.stereo ? 2 : 1);
}

uint32_t i2s_driver_get_sample_rate(i2s_driver_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }
    return ((i2s_driver_t *)handle)->config.sample_rate;
}

uint8_t i2s_driver_get_bits_per_sample(i2s_driver_handle_t handle)
{
    if (handle == NULL) {
        return 0;
    }
    return ((i2s_driver_t *)handle)->config.bits_per_sample;
}

bool i2s_driver_pause(i2s_driver_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    i2s_driver_t *drv = (i2s_driver_t *)handle;
    return i2s_channel_disable(drv->tx_channel) == ESP_OK;
}

bool i2s_driver_resume(i2s_driver_handle_t handle)
{
    if (handle == NULL) {
        return false;
    }

    i2s_driver_t *drv = (i2s_driver_t *)handle;
    return i2s_channel_enable(drv->tx_channel) == ESP_OK;
}

i2s_chan_handle_t i2s_driver_get_channel(i2s_driver_handle_t handle)
{
    if (handle == NULL) {
        return NULL;
    }
    return ((i2s_driver_t *)handle)->tx_channel;
}