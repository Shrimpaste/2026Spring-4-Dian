/*
 * Level-1 UART 串口通信示例
 *
 * 功能说明:
 * - 初始化UART0（默认串口，用于与电脑通信）
 * - 初始化UART1（可选，用于与其他设备通信）
 * - 实现数据回显：将收到的数据原样发送回去
 * - 支持命令解析：输入"on"开灯，"off"关灯（使用板载LED）
 *
 * 学习目标:
 * - 理解UART通信基本原理
 * - 掌握ESP-IDF UART驱动API的使用
 * - 学习串口数据接收与发送
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "UART";

// UART配置参数
#define UART_NUM            UART_NUM_0      // 使用UART0（默认USB串口）
#define UART_TX_PIN         43              // ESP32-S3默认UART0 TX引脚
#define UART_RX_PIN         44              // ESP32-S3默认UART0 RX引脚
#define UART_BAUD_RATE      115200          // 波特率
#define BUF_SIZE            1024            // 缓冲区大小

// 板载LED引脚（ESP32-S3-DevKitC-1为GPIO2）
#define LED_GPIO            GPIO_NUM_2

// 命令定义
#define CMD_ON              "on"
#define CMD_OFF             "off"
#define CMD_HELP            "help"

static void uart_init(void)
{
    // UART配置结构体
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 安装UART驱动
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));

    // 配置UART参数
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));

    // 设置引脚（对于UART0，通常使用默认引脚，这里显式设置以防万一）
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART初始化完成，波特率: %d", UART_BAUD_RATE);
}

static void led_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 0);
}

static void process_command(const char *cmd)
{
    // 去除末尾换行符
    char clean_cmd[32];
    strncpy(clean_cmd, cmd, sizeof(clean_cmd) - 1);
    clean_cmd[sizeof(clean_cmd) - 1] = '\0';

    // 去除\r和\n
    char *p = clean_cmd;
    while (*p) {
        if (*p == '\r' || *p == '\n') {
            *p = '\0';
            break;
        }
        p++;
    }

    ESP_LOGI(TAG, "收到命令: %s", clean_cmd);

    if (strcasecmp(clean_cmd, CMD_ON) == 0) {
        gpio_set_level(LED_GPIO, 1);
        uart_write_bytes(UART_NUM, "LED 已开启\r\n", strlen("LED 已开启\r\n"));
        ESP_LOGI(TAG, "LED 开启");
    }
    else if (strcasecmp(clean_cmd, CMD_OFF) == 0) {
        gpio_set_level(LED_GPIO, 0);
        uart_write_bytes(UART_NUM, "LED 已关闭\r\n", strlen("LED 已关闭\r\n"));
        ESP_LOGI(TAG, "LED 关闭");
    }
    else if (strcasecmp(clean_cmd, CMD_HELP) == 0) {
        const char *help_msg = "\r\n=== UART 控制命令 ===\r\n"
                               "on   - 开启LED\r\n"
                               "off  - 关闭LED\r\n"
                               "help - 显示帮助\r\n"
                               "其他 - 回显输入\r\n"
                               "==================\r\n";
        uart_write_bytes(UART_NUM, help_msg, strlen(help_msg));
    }
    else {
        // 回显模式
        char echo_buf[128];
        snprintf(echo_buf, sizeof(echo_buf), "回显: %s\r\n", clean_cmd);
        uart_write_bytes(UART_NUM, echo_buf, strlen(echo_buf));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Level-1 UART 串口通信示例 ===");

    // 初始化LED
    led_init();

    // 初始化UART
    uart_init();

    // 发送欢迎信息
    const char *welcome = "\r\n欢迎使用ESP32 UART通信示例!\r\n"
                          "输入 'help' 查看可用命令\r\n"
                          ">> ";
    uart_write_bytes(UART_NUM, welcome, strlen(welcome));

    uint8_t data[BUF_SIZE];
    int pos = 0;

    while (1) {
        // 读取串口数据
        int len = uart_read_bytes(UART_NUM, data + pos, 1, pdMS_TO_TICKS(100));

        if (len > 0) {
            // 回显字符
            uart_write_bytes(UART_NUM, data + pos, 1);

            // 检测换行（命令结束）
            if (data[pos] == '\r' || data[pos] == '\n') {
                if (pos > 0) {
                    data[pos] = '\0';
                    process_command((char *)data);
                    pos = 0;
                }
                uart_write_bytes(UART_NUM, ">> ", 3);
            }
            else {
                pos++;
                if (pos >= BUF_SIZE - 1) {
                    pos = 0;
                }
            }
        }
    }
}
