/**
 * @file i2s_audio.c
 * @brief I2S audio output implementation for MAX98357A
 *
 * MAX98357A connections:
 * - BCLK (Bit Clock) -> GPIO4
 * - LRCK (Left-Right Clock / WS) -> GPIO5
 * - DIN (Data Input) -> GPIO6
 * - SD_MODE (Shutdown/Mode) -> GPIO7 (high = enabled, low = shutdown)
 * - GND -> GND
 * - VIN -> 3.3V or 5V
 *
 * Note: MAX98357A is a mono amplifier that mixes left and right channels
 */

#include "i2s_audio.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include <math.h>

static const char *TAG = "I2S_AUDIO";

static i2s_chan_handle_t s_tx_chan = NULL;
static int s_volume = 70;  /* Default volume: 70% */
static bool s_mute = false;
static bool s_playing = false;
static bool s_initialized = false;
static uint32_t s_position = 0;
static SemaphoreHandle_t s_audio_mutex = NULL;

esp_err_t i2s_audio_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "I2S already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing I2S for MAX98357A...");
    ESP_LOGI(TAG, "BCLK: GPIO%d, LRCK: GPIO%d, DATA: GPIO%d, SD_MODE: GPIO%d",
             CONFIG_MP3_I2S_BCLK_GPIO, CONFIG_MP3_I2S_LRCK_GPIO,
             CONFIG_MP3_I2S_DATA_GPIO, CONFIG_MP3_I2S_SD_MODE_GPIO);

    /* Create mutex */
    s_audio_mutex = xSemaphoreCreateMutex();
    if (s_audio_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Configure SD_MODE pin for MAX98357A shutdown control */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_MP3_I2S_SD_MODE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    /* Start with amplifier in shutdown mode */
    gpio_set_level(CONFIG_MP3_I2S_SD_MODE_GPIO, 0);

    /* Allocate I2S TX channel */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;  /* Auto clear TX buffer when underflow */

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx_chan, NULL));

    /* Configure I2S standard mode for MAX98357A */
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_MP3_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = CONFIG_MP3_I2S_BCLK_GPIO,
            .ws = CONFIG_MP3_I2S_LRCK_GPIO,
            .dout = CONFIG_MP3_I2S_DATA_GPIO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx_chan, &std_cfg));

    s_initialized = true;
    ESP_LOGI(TAG, "I2S initialized successfully");

    return ESP_OK;
}

esp_err_t i2s_audio_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    i2s_audio_stop_playback();
    i2s_audio_set_enable(false);

    ESP_ERROR_CHECK(i2s_del_channel(s_tx_chan));
    s_tx_chan = NULL;

    if (s_audio_mutex) {
        vSemaphoreDelete(s_audio_mutex);
        s_audio_mutex = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "I2S deinitialized");
    return ESP_OK;
}

esp_err_t i2s_audio_start(void)
{
    if (!s_initialized || s_tx_chan == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_ERROR_CHECK(i2s_channel_enable(s_tx_chan));
    i2s_audio_set_enable(true);

    return ESP_OK;
}

esp_err_t i2s_audio_stop(void)
{
    if (!s_initialized || s_tx_chan == NULL) {
        return ESP_OK;
    }

    i2s_audio_set_enable(false);
    ESP_ERROR_CHECK(i2s_channel_disable(s_tx_chan));

    return ESP_OK;
}

esp_err_t i2s_audio_write(const uint8_t *data, size_t len, size_t *bytes_written)
{
    if (!s_initialized || s_tx_chan == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_audio_mutex, portMAX_DELAY);

    /* Apply volume scaling if needed */
    static int16_t vol_buffer[CONFIG_MP3_BUFFER_SIZE / 2];
    size_t samples = len / 2;
    if (samples > sizeof(vol_buffer) / sizeof(vol_buffer[0])) {
        samples = sizeof(vol_buffer) / sizeof(vol_buffer[0]);
    }

    if (s_mute) {
        memset(vol_buffer, 0, samples * 2);
    } else if (s_volume < 100) {
        /* Apply volume scaling */
        const int16_t *src = (const int16_t *)data;
        for (size_t i = 0; i < samples; i++) {
            vol_buffer[i] = (int16_t)((src[i] * s_volume) / 100);
        }
    } else {
        memcpy(vol_buffer, data, samples * 2);
    }

    size_t written = 0;
    esp_err_t ret = i2s_channel_write(s_tx_chan, vol_buffer, samples * 2, &written, portMAX_DELAY);

    xSemaphoreGive(s_audio_mutex);

    if (bytes_written) {
        *bytes_written = written;
    }

    if (ret == ESP_OK) {
        s_position += written;
    }

    return ret;
}

esp_err_t i2s_audio_set_sample_rate(uint32_t sample_rate)
{
    if (!s_initialized || s_tx_chan == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(s_tx_chan, &clk_cfg));

    ESP_LOGI(TAG, "Sample rate changed to %lu Hz", sample_rate);
    return ESP_OK;
}

esp_err_t i2s_audio_set_enable(bool enable)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    gpio_set_level(CONFIG_MP3_I2S_SD_MODE_GPIO, enable ? 1 : 0);
    ESP_LOGI(TAG, "Amplifier %s", enable ? "enabled" : "shutdown");
    return ESP_OK;
}

esp_err_t i2s_audio_set_volume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    xSemaphoreTake(s_audio_mutex, portMAX_DELAY);
    s_volume = volume;
    xSemaphoreGive(s_audio_mutex);

    ESP_LOGI(TAG, "Volume set to %d%%", s_volume);
    return ESP_OK;
}

int i2s_audio_get_volume(void)
{
    return s_volume;
}

esp_err_t i2s_audio_set_mute(bool mute)
{
    xSemaphoreTake(s_audio_mutex, portMAX_DELAY);
    s_mute = mute;
    xSemaphoreGive(s_audio_mutex);

    ESP_LOGI(TAG, "%s", mute ? "Muted" : "Unmuted");
    return ESP_OK;
}

esp_err_t i2s_audio_play_mp3(const char *filepath, uint32_t start_position)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "I2S not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Stop any current playback */
    i2s_audio_stop_playback();

    /* Open the file */
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }

    /* Seek to start position if needed */
    if (start_position > 0) {
        fseek(fp, start_position, SEEK_SET);
        ESP_LOGI(TAG, "Resuming from position %lu", start_position);
    }

    /* Start I2S */
    ESP_ERROR_CHECK(i2s_audio_start());
    s_playing = true;
    s_position = start_position;

    ESP_LOGI(TAG, "Started playing: %s", filepath);

    /* Note: Actual MP3 decoding should be done in a separate task */
    /* This is just the I2S interface layer */

    fclose(fp);
    return ESP_OK;
}

esp_err_t i2s_audio_pause(void)
{
    if (!s_playing) {
        return ESP_OK;
    }

    i2s_audio_set_enable(false);
    s_playing = false;

    ESP_LOGI(TAG, "Playback paused at position %lu", s_position);
    return ESP_OK;
}

esp_err_t i2s_audio_resume(void)
{
    if (s_playing) {
        return ESP_OK;
    }

    i2s_audio_set_enable(true);
    s_playing = true;

    ESP_LOGI(TAG, "Playback resumed");
    return ESP_OK;
}

esp_err_t i2s_audio_stop_playback(void)
{
    s_playing = false;
    s_position = 0;
    i2s_audio_set_enable(false);

    ESP_LOGI(TAG, "Playback stopped");
    return ESP_OK;
}

uint32_t i2s_audio_get_position(void)
{
    return s_position;
}

bool i2s_audio_is_playing(void)
{
    return s_playing;
}
