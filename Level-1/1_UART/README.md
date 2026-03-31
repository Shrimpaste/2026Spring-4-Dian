# Level-1 UART 串口通信实验

ESP32-S3 UART 串口通信实验，实现定时发送和接收响应功能。

## 实验要求

1. 芯片以 **1Hz 速率**通过 UART 发送 `Hello World\r\n`
2. 接收用户输入，当用户按下**回车**时，立即输出：
   ```
   GEL37KXHDU9G\r\n
   FXLKNKWHVURC\r\n
   CE4K7KEYCUPQ\r\n
   ```

## 硬件要求

- **开发板**: ESP32-S3
- **连接**: USB线直连（使用板载USB转串口）

| 信号 | GPIO | 说明 |
|------|------|------|
| UART TX | GPIO43 | UART0发送 |
| UART RX | GPIO44 | UART0接收 |

## 编译与烧录

```bash
# 设置目标芯片
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录并监控（将PORT替换为实际串口号）
idf.py -p PORT flash monitor
```

## 串口设置

| 参数 | 值 |
|------|-----|
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |

## 运行示例

```
Hello World
Hello World
[按回车]
GEL37KXHDU9G
FXLKNKWHVURC
CE4K7KEYCUPQ
Hello World
[输入abc按回车]
abcGEL37KXHDU9G
FXLKNKWHVURC
CE4K7KEYCUPQ
Hello World
```

## 核心代码

```c
// UART初始化
uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
};
uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);
uart_param_config(UART_NUM_0, &uart_config);
uart_set_pin(UART_NUM_0, 43, 44, -1, -1);

// 1Hz发送
if (xTaskGetTickCount() - last_time >= pdMS_TO_TICKS(1000)) {
    uart_write_bytes(UART_NUM_0, "Hello World\r\n", 14);
}

// 接收并响应回车
if (ch == '\r' || ch == '\n') {
    uart_write_bytes(UART_NUM_0, "GEL37KXHDU9G\r\n", 15);
    uart_write_bytes(UART_NUM_0, "FXLKNKWHVURC\r\n", 15);
    uart_write_bytes(UART_NUM_0, "CE4K7KEYCUPQ\r\n", 15);
    pos = 0;
}
else {
    pos++;
}
```

## 参考资料

- [ESP-IDF UART驱动文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/peripherals/uart.html)
