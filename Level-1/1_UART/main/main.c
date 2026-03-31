/*
 * Level-1 UART 实验
 * 1. 以1Hz速率发送 "Hello World\r\n"
 * 2. 接收用户输入，当用户按下回车时，立即输出指定字符串
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "UART";

#define UART_NUM        UART_NUM_0
#define UART_TX_PIN     43
#define UART_RX_PIN     44
#define UART_BAUD_RATE  115200
#define BUF_SIZE        1024

static void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART init done, baud: %d", UART_BAUD_RATE);
}

void app_main(void)
{
    uart_init();

    uint8_t data[BUF_SIZE];
    int pos = 0;
    TickType_t last_send_time = xTaskGetTickCount();

    while (1) {
        // 1Hz 发送 "Hello World\r\n"
        if (xTaskGetTickCount() - last_send_time >= pdMS_TO_TICKS(1000)) {
            uart_write_bytes(UART_NUM, "Hello World\r\n", strlen("Hello World\r\n"));
            last_send_time = xTaskGetTickCount();
        }

        // 接收用户输入
        int len = uart_read_bytes(UART_NUM, &data[pos], 1, pdMS_TO_TICKS(50));

        if (len > 0) {
            uint8_t ch = data[pos];

            // 检测回车 (\r 或 \n)
            if (ch == '\r' || ch == '\n') {
                // 当用户按下回车时，立即输出指定字符串
                uart_write_bytes(UART_NUM, "GEL37KXHDU9G\r\n", strlen("GEL37KXHDU9G\r\n"));
                uart_write_bytes(UART_NUM, "FXLKNKWHVURC\r\n", strlen("FXLKNKWHVURC\r\n"));
                uart_write_bytes(UART_NUM, "CE4K7KEYCUPQ\r\n", strlen("CE4K7KEYCUPQ\r\n"));
                pos = 0;
            }
            else {
                // 普通字符，继续接收
                pos++;
                if (pos >= BUF_SIZE - 1) {
                    pos = 0;
                }
            }
        }
    }
}