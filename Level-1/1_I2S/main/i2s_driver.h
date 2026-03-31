/**
 * @file i2s_driver.h
 * @brief I2S音频驱动库
 *
 * 封装ESP32 I2S外设的初始化和数据传输功能
 * 支持标准I2S模式，可配置采样率、位深和引脚
 */

#ifndef I2S_DRIVER_H
#define I2S_DRIVER_H

#include "driver/i2s_std.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I2S驱动配置结构体
 */
typedef struct {
    uint32_t sample_rate;       /**< 采样率(Hz)，如44100, 48000 */
    uint8_t  bits_per_sample;   /**< 位深，如16, 24, 32 */
    int      gpio_ws;           /**< WS/LRC引脚 */
    int      gpio_bclk;         /**< BCLK引脚 */
    int      gpio_dout;         /**< 数据输出引脚 */
    bool     stereo;            /**< true=立体声, false=单声道 */
} i2s_driver_config_t;

/**
 * @brief I2S驱动句柄
 */
typedef struct i2s_driver_s* i2s_driver_handle_t;

/**
 * @brief 默认I2S驱动配置(GPIO5/6/7, 44.1kHz, 16bit, 立体声)
 */
#define I2S_DRIVER_DEFAULT_CONFIG() { \
    .sample_rate = 44100, \
    .bits_per_sample = 16, \
    .gpio_ws = 5, \
    .gpio_bclk = 6, \
    .gpio_dout = 7, \
    .stereo = true, \
}

/**
 * @brief 初始化I2S驱动
 * @param config 配置参数，传NULL使用默认配置
 * @return 驱动句柄，失败返回NULL
 */
i2s_driver_handle_t i2s_driver_init(const i2s_driver_config_t *config);

/**
 * @brief 反初始化I2S驱动
 * @param handle 驱动句柄
 * @return true=成功, false=失败
 */
bool i2s_driver_deinit(i2s_driver_handle_t handle);

/**
 * @brief 向I2S发送音频数据(阻塞模式)
 * @param handle 驱动句柄
 * @param buffer 音频数据缓冲区
 * @param sample_count 样本帧数(立体声为L+R对数)
 * @param timeout_ms 超时时间(ms)，0表示不等待，-1表示永久等待
 * @return 实际发送的样本帧数，<0表示错误
 */
int i2s_driver_write(i2s_driver_handle_t handle,
                     const int16_t *buffer,
                     int sample_count,
                     int timeout_ms);

/**
 * @brief 获取当前采样率
 * @param handle 驱动句柄
 * @return 采样率(Hz)
 */
uint32_t i2s_driver_get_sample_rate(i2s_driver_handle_t handle);

/**
 * @brief 获取当前位深
 * @param handle 驱动句柄
 * @return 位深(bits)
 */
uint8_t i2s_driver_get_bits_per_sample(i2s_driver_handle_t handle);

/**
 * @brief 暂停I2S传输
 * @param handle 驱动句柄
 * @return true=成功, false=失败
 */
bool i2s_driver_pause(i2s_driver_handle_t handle);

/**
 * @brief 恢复I2S传输
 * @param handle 驱动句柄
 * @return true=成功, false=失败
 */
bool i2s_driver_resume(i2s_driver_handle_t handle);

/**
 * @brief 获取I2S通道句柄(用于直接调用ESP-IDF API)
 * @param handle 驱动句柄
 * @return I2S通道句柄
 */
i2s_chan_handle_t i2s_driver_get_channel(i2s_driver_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* I2S_DRIVER_H */