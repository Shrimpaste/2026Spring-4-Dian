/**
 * @file audio_generator.h
 * @brief 音频波形生成库
 *
 * 提供正弦波、锯齿波等常见波形的生成功能
 * 支持立体声/单声道输出，可独立控制左右声道频率
 */

#ifndef AUDIO_GENERATOR_H
#define AUDIO_GENERATOR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 声道模式枚举
 */
typedef enum {
    CHANNEL_LEFT_ONLY  = 0,  /**< 仅左声道 */
    CHANNEL_RIGHT_ONLY = 1,  /**< 仅右声道 */
    CHANNEL_BOTH       = 2,  /**< 双声道相同信号 */
    CHANNEL_INDEPENDENT = 3, /**< 左右声道独立频率 */
} channel_mode_t;

/**
 * @brief 音频生成器配置结构体
 */
typedef struct {
    uint32_t sample_rate;       /**< 采样率(Hz) */
    int16_t  amplitude;         /**< 信号幅度(0-32767) */
} audio_gen_config_t;

/**
 * @brief 音频生成器句柄
 */
typedef struct audio_generator_s* audio_generator_handle_t;

/**
 * @brief 默认音频生成器配置
 */
#define AUDIO_GEN_DEFAULT_CONFIG() { \
    .sample_rate = 44100, \
    .amplitude = 8000, \
}

/**
 * @brief 创建音频生成器
 * @param config 配置参数
 * @return 生成器句柄，失败返回NULL
 */
audio_generator_handle_t audio_gen_create(const audio_gen_config_t *config);

/**
 * @brief 销毁音频生成器
 * @param gen 生成器句柄
 */
void audio_gen_destroy(audio_generator_handle_t gen);

/**
 * @brief 生成正弦波
 * @param gen 生成器句柄
 * @param buffer 输出缓冲区(左右交替存储)
 * @param sample_count 样本帧数(立体声为sample_count*2个int16)
 * @param freq_left 左声道频率(Hz)
 * @param freq_right 右声道频率(Hz)，仅在CHANNEL_INDEPENDENT模式下有效
 * @param mode 声道模式
 * @return 实际生成的样本帧数
 */
int audio_gen_sine_wave(audio_generator_handle_t gen,
                        int16_t *buffer,
                        int sample_count,
                        int freq_left,
                        int freq_right,
                        channel_mode_t mode);

/**
 * @brief 生成锯齿波
 * @param gen 生成器句柄
 * @param buffer 输出缓冲区
 * @param sample_count 样本帧数
 * @param freq 频率(Hz)
 * @param mode 声道模式
 * @return 实际生成的样本帧数
 */
int audio_gen_sawtooth_wave(audio_generator_handle_t gen,
                            int16_t *buffer,
                            int sample_count,
                            int freq,
                            channel_mode_t mode);

/**
 * @brief 生成静音(填充0)
 * @param buffer 输出缓冲区
 * @param sample_count 样本帧数
 * @return 实际生成的样本帧数
 */
int audio_gen_silence(int16_t *buffer, int sample_count);

/**
 * @brief 设置新的幅度
 * @param gen 生成器句柄
 * @param amplitude 新幅度值(0-32767)
 */
void audio_gen_set_amplitude(audio_generator_handle_t gen, int16_t amplitude);

/**
 * @brief 获取当前幅度
 * @param gen 生成器句柄
 * @return 当前幅度值
 */
int16_t audio_gen_get_amplitude(audio_generator_handle_t gen);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_GENERATOR_H */