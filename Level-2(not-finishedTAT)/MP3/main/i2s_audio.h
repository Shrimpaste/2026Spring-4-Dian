/**
 * @file i2s_audio.h
 * @brief I2S audio output for MAX98357A
 */

#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include "mp3_player.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2S_PORT        I2S_NUM_0
#define I2S_BUF_COUNT   4

/**
 * @brief I2S configuration for MAX98357A
 *
 * MAX98357A connections:
 * - BCLK (Bit Clock) -> GPIO4
 * - LRCK (Left-Right Clock / WS) -> GPIO5
 * - DIN (Data Input) -> GPIO6
 * - SD_MODE (Shutdown/Mode) -> GPIO7 (high = enabled)
 * - GND -> GND
 * - VIN -> 3.3V or 5V (module has built-in regulator)
 */

/**
 * @brief Initialize I2S interface for MAX98357A
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_init(void);

/**
 * @brief Deinitialize I2S
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_deinit(void);

/**
 * @brief Start I2S audio output
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_start(void);

/**
 * @brief Stop I2S audio output
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_stop(void);

/**
 * @brief Write audio data to I2S
 * @param[in] data Audio data buffer
 * @param[in] len Data length in bytes
 * @param[out] bytes_written Actual bytes written (can be NULL)
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_write(const uint8_t *data, size_t len, size_t *bytes_written);

/**
 * @brief Set audio sample rate
 * @param[in] sample_rate Sample rate in Hz (8000-48000)
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_set_sample_rate(uint32_t sample_rate);

/**
 * @brief Enable/disable MAX98357A amplifier
 * @param[in] enable true to enable, false to shutdown
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_set_enable(bool enable);

/**
 * @brief Set volume (not directly supported by MAX98357A, adjust in software)
 * @param[in] volume Volume level 0-100
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_set_volume(int volume);

/**
 * @brief Get current volume
 * @return Current volume level 0-100
 */
int i2s_audio_get_volume(void);

/**
 * @brief Mute/unmute audio
 * @param[in] mute true to mute, false to unmute
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_set_mute(bool mute);

/**
 * @brief Play MP3 file
 * @param[in] filepath Full path to MP3 file
 * @param[in] start_position Byte position to start from (for resume)
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_play_mp3(const char *filepath, uint32_t start_position);

/**
 * @brief Pause playback
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_pause(void);

/**
 * @brief Resume playback
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_resume(void);

/**
 * @brief Stop playback
 * @return ESP_OK on success
 */
esp_err_t i2s_audio_stop_playback(void);

/**
 * @brief Get current playback position
 * @return Current position in bytes
 */
uint32_t i2s_audio_get_position(void);

/**
 * @brief Check if currently playing
 * @return true if playing, false otherwise
 */
bool i2s_audio_is_playing(void);

#ifdef __cplusplus
}
#endif

#endif /* I2S_AUDIO_H */
