/**
 * @file config_manager.c
 * @brief Configuration management using NVS
 */

#include "mp3_player.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "CONFIG";

esp_err_t config_load(player_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Load player name */
    size_t len = MAX_PLAYER_NAME_LEN;
    ret = nvs_get_str(handle, NVS_KEY_PLAYER_NAME, config->player_name, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        /* First boot - no name set */
        config->is_first_boot = true;
        config->player_name[0] = '\0';
        ESP_LOGI(TAG, "First boot detected - player name not set");
    } else if (ret == ESP_OK) {
        config->is_first_boot = false;
        ESP_LOGI(TAG, "Loaded player name: %s", config->player_name);
    }

    /* Load volume (use default if not found) */
    int32_t vol = 70;
    ret = nvs_get_i32(handle, "volume", &vol);
    if (ret == ESP_OK) {
        config->volume = (int)vol;
    } else {
        config->volume = 70;  /* Default volume */
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t config_save(const player_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Save player name */
    ESP_ERROR_CHECK(nvs_set_str(handle, NVS_KEY_PLAYER_NAME, config->player_name));
    ESP_ERROR_CHECK(nvs_set_i32(handle, "volume", config->volume));

    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);

    ESP_LOGI(TAG, "Configuration saved");
    return ESP_OK;
}

esp_err_t config_set_player_name(const char *name)
{
    nvs_handle_t handle;
    esp_err_t ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_ERROR_CHECK(nvs_set_str(handle, NVS_KEY_PLAYER_NAME, name));
    ESP_ERROR_CHECK(nvs_set_u8(handle, "first_boot", 0));
    ESP_ERROR_CHECK(nvs_commit(handle));

    nvs_close(handle);

    /* Update global config */
    strncpy(g_player.config.player_name, name, MAX_PLAYER_NAME_LEN - 1);
    g_player.config.player_name[MAX_PLAYER_NAME_LEN - 1] = '\0';
    g_player.config.is_first_boot = false;

    ESP_LOGI(TAG, "Player name set to: %s", name);
    return ESP_OK;
}

esp_err_t config_save_playback_state(int song_index, uint32_t position)
{
    nvs_handle_t handle;
    esp_err_t ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_ERROR_CHECK(nvs_set_i32(handle, NVS_KEY_SONG_INDEX, song_index));
    ESP_ERROR_CHECK(nvs_set_u32(handle, NVS_KEY_PLAYBACK_POS, position));
    ESP_ERROR_CHECK(nvs_commit(handle));

    nvs_close(handle);

    ESP_LOGI(TAG, "Playback state saved: song=%d, pos=%lu", song_index, position);
    return ESP_OK;
}

esp_err_t config_load_playback_state(int *song_index, uint32_t *position)
{
    nvs_handle_t handle;
    esp_err_t ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        *song_index = 0;
        *position = 0;
        return ret;
    }

    int32_t idx = 0;
    ret = nvs_get_i32(handle, NVS_KEY_SONG_INDEX, &idx);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        idx = 0;
    }

    uint32_t pos = 0;
    ret = nvs_get_u32(handle, NVS_KEY_PLAYBACK_POS, &pos);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        pos = 0;
    }

    nvs_close(handle);

    *song_index = (int)idx;
    *position = pos;

    ESP_LOGI(TAG, "Playback state loaded: song=%d, pos=%lu", *song_index, *position);
    return ESP_OK;
}
