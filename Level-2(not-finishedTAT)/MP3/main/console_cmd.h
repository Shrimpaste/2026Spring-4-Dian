/**
 * @file console_cmd.h
 * @brief Console commands for MP3 player control
 */

#ifndef CONSOLE_CMD_H
#define CONSOLE_CMD_H

#include "mp3_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize console and register commands
 * @return ESP_OK on success
 */
esp_err_t console_cmd_init(void);

/**
 * @brief Start console task for command processing
 * @return ESP_OK on success
 */
esp_err_t console_cmd_start(void);

/**
 * @brief Print help information
 */
void console_cmd_print_help(void);

/**
 * @brief Process a single command string
 * @param[in] cmdline Command line string
 */
void console_cmd_process(const char *cmdline);

/**
 * @brief Print welcome message and player info
 */
void console_cmd_print_welcome(void);

/**
 * @brief Print current player status
 */
void console_cmd_print_status(void);

/**
 * @brief Print song list
 */
void console_cmd_print_songs(void);

/**
 * @brief Deinitialize console
 * @return ESP_OK on success
 */
esp_err_t console_cmd_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_CMD_H */
