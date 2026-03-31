/*
 * Level-1 UART 实验 - 有别于main逐字读取
 * 缓冲区内查找\r\n
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

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
}

void app_main(void)
{
    uart_init();

    uint8_t rx_buf[BUF_SIZE];
    int rx_len = 0;  // 当前缓冲区已有数据长度
    TickType_t last_send_time = xTaskGetTickCount();

    while (1) {
        // 1Hz 发送 "Hello World\r\n"
        if (xTaskGetTickCount() - last_send_time >= pdMS_TO_TICKS(1000)) {
            uart_write_bytes(UART_NUM, "Hello World\r\n", strlen("Hello World\r\n"));
            last_send_time = xTaskGetTickCount();
        }

        // 批量读取新数据（非阻塞）
        int len = uart_read_bytes(UART_NUM, rx_buf + rx_len, BUF_SIZE - rx_len - 1, pdMS_TO_TICKS(50));

        if (len > 0) {
            rx_len += len;
            rx_buf[rx_len] = '\0';

            // 查找 \r 或 \n
            for (int i = 0; i < rx_len; i++) {
                if (rx_buf[i] == '\r' || rx_buf[i] == '\n') {
                    // 找到回车，输出
                    uart_write_bytes(UART_NUM, "GEL37KXHDU9G\r\n", strlen("GEL37KXHDU9G\r\n"));
                    uart_write_bytes(UART_NUM, "FXLKNKWHVURC\r\n", strlen("FXLKNKWHVURC\r\n"));
                    uart_write_bytes(UART_NUM, "CE4K7KEYCUPQ\r\n", strlen("CE4K7KEYCUPQ\r\n"));

                    // 将剩余数据移到缓冲区开头
                    int remaining = rx_len - i - 1;
                    if (remaining > 0) {
                        memmove(rx_buf, rx_buf + i + 1, remaining);
                    }
                    rx_len = remaining;
                    break;  // 处理完一个回车，跳出循环
                }
            }

            // 防止缓冲区溢出
            if (rx_len >= BUF_SIZE - 1) {
                rx_len = 0;
            }
        }
    }
}
/*
    此处为逐个处理\n，处理一次后更新数据，跳出循环，待下次查找
    亦可以在while中持续查找，直到没有\n为止，但需要注意更新数据和长度，避免死循环
*/