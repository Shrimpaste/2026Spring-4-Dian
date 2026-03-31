/**
 * @file audio_generator.c
 * @brief 音频波形生成库实现
 */

#include "audio_generator.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief 音频生成器实例结构体
 */
typedef struct audio_generator_s {
    audio_gen_config_t config;  /**< 配置参数 */
    float phase_left;           /**< 左声道相位(0-1) */
    float phase_right;          /**< 右声道相位(0-1) */
    float phase_sawtooth;       /**< 锯齿波相位 */
} audio_generator_t;

audio_generator_handle_t audio_gen_create(const audio_gen_config_t *config)
{
    audio_generator_t *gen = (audio_generator_t *)malloc(sizeof(audio_generator_t));
    if (gen == NULL) {
        return NULL;
    }

    if (config != NULL) {
        memcpy(&gen->config, config, sizeof(audio_gen_config_t));
    } else {
        gen->config = (audio_gen_config_t)AUDIO_GEN_DEFAULT_CONFIG();
    }

    gen->phase_left = 0.0f;
    gen->phase_right = 0.0f;
    gen->phase_sawtooth = 0.0f;

    return (audio_generator_handle_t)gen;
}

void audio_gen_destroy(audio_generator_handle_t gen)
{
    if (gen != NULL) {
        free(gen);
    }
}

int audio_gen_sine_wave(audio_generator_handle_t gen,
                        int16_t *buffer,
                        int sample_count,
                        int freq_left,
                        int freq_right,
                        channel_mode_t mode)
{
    if (gen == NULL || buffer == NULL || sample_count <= 0) {
        return 0;
    }

    audio_generator_t *g = (audio_generator_t *)gen;
    float amp = (float)g->config.amplitude;
    float sr = (float)g->config.sample_rate;

    float phase_inc_left = (float)freq_left / sr;
    float phase_inc_right = (float)freq_right / sr;

    for (int i = 0; i < sample_count; i++) {
        int16_t left_sample = 0;
        int16_t right_sample = 0;

        switch (mode) {
            case CHANNEL_LEFT_ONLY:
                left_sample = (int16_t)(amp * sinf(2.0f * M_PI * g->phase_left));
                break;

            case CHANNEL_RIGHT_ONLY:
                right_sample = (int16_t)(amp * sinf(2.0f * M_PI * g->phase_right));
                break;

            case CHANNEL_BOTH:
                left_sample = (int16_t)(amp * sinf(2.0f * M_PI * g->phase_left));
                right_sample = left_sample;
                g->phase_right += phase_inc_right;
                if (g->phase_right >= 1.0f) g->phase_right -= 1.0f;
                break;

            case CHANNEL_INDEPENDENT:
                left_sample = (int16_t)(amp * sinf(2.0f * M_PI * g->phase_left));
                right_sample = (int16_t)(amp * sinf(2.0f * M_PI * g->phase_right));
                g->phase_right += phase_inc_right;
                if (g->phase_right >= 1.0f) g->phase_right -= 1.0f;
                break;

            default:
                break;
        }

        buffer[i * 2] = left_sample;
        buffer[i * 2 + 1] = right_sample;

        g->phase_left += phase_inc_left;
        if (g->phase_left >= 1.0f) g->phase_left -= 1.0f;
    }

    return sample_count;
}

int audio_gen_sawtooth_wave(audio_generator_handle_t gen,
                            int16_t *buffer,
                            int sample_count,
                            int freq,
                            channel_mode_t mode)
{
    if (gen == NULL || buffer == NULL || sample_count <= 0) {
        return 0;
    }

    audio_generator_t *g = (audio_generator_t *)gen;
    float amp = (float)g->config.amplitude;
    float sr = (float)g->config.sample_rate;

    float phase_inc = (float)freq / sr;

    for (int i = 0; i < sample_count; i++) {
        // 锯齿波: -1.0 ~ 1.0 线性变化
        float sawtooth = 2.0f * g->phase_sawtooth - 1.0f;
        int16_t sample = (int16_t)(amp * sawtooth);

        switch (mode) {
            case CHANNEL_LEFT_ONLY:
                buffer[i * 2] = sample;
                buffer[i * 2 + 1] = 0;
                break;

            case CHANNEL_RIGHT_ONLY:
                buffer[i * 2] = 0;
                buffer[i * 2 + 1] = sample;
                break;

            case CHANNEL_BOTH:
            case CHANNEL_INDEPENDENT:
                buffer[i * 2] = sample;
                buffer[i * 2 + 1] = sample;
                break;

            default:
                buffer[i * 2] = 0;
                buffer[i * 2 + 1] = 0;
                break;
        }

        g->phase_sawtooth += phase_inc;
        if (g->phase_sawtooth >= 1.0f) {
            g->phase_sawtooth -= 1.0f;
        }
    }

    return sample_count;
}

int audio_gen_silence(int16_t *buffer, int sample_count)
{
    if (buffer == NULL || sample_count <= 0) {
        return 0;
    }

    memset(buffer, 0, sample_count * sizeof(int16_t) * 2);
    return sample_count;
}

void audio_gen_set_amplitude(audio_generator_handle_t gen, int16_t amplitude)
{
    if (gen != NULL) {
        ((audio_generator_t *)gen)->config.amplitude = amplitude;
    }
}

int16_t audio_gen_get_amplitude(audio_generator_handle_t gen)
{
    if (gen != NULL) {
        return ((audio_generator_t *)gen)->config.amplitude;
    }
    return 0;
}