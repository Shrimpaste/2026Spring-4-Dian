# Level-1 UART 串口通信示例

ESP32 UART串口通信基础教程，实现与电脑的串口通信，支持命令控制和数据回显。

## 学习目标

- 理解UART通信基本原理（波特率、数据位、停止位、校验位）
- 掌握ESP-IDF UART驱动API的使用
- 学习串口数据接收、发送和命令解析

## 硬件连接

ESP32-S3-DevKitC-1开发板通过USB线连接电脑即可，使用板载USB转串口功能。

| 信号 | GPIO | 说明 |
|------|------|------|
| UART TX | GPIO43 | 默认UART0发送 |
| UART RX | GPIO44 | 默认UART0接收 |
| LED | GPIO2 | 接led |

## 软件要求

1. 串口调试工具（推荐）：
   - Windows: [SSCOM](https://github.com/Neutree/SCOMM)、PuTTY、XCOM
   - Linux: `minicom` 或 `screen`
   - 通用: [串口助手网页版](https://serial.hyperos.mi.com/)

2. 波特率设置：**115200**

## 编译与烧录

```bash
# 设置目标芯片
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录并监控（自动使用UART0输出日志）
idf.py -p /dev/ttyUSB0 flash monitor
```

## 使用说明

### 1. 基本通信测试
打开串口调试工具，连接开发板对应的串口，设置波特率115200。输入任意字符，开发板会回显你的输入。

### 2. LED控制命令
- 输入 `on` 开启LED
- 输入 `off` 关闭LED
- 输入 `help` 显示帮助信息

### 3. 示例交互
```
欢迎使用ESP32 UART通信示例!
输入 'help' 查看可用命令
>> help

=== UART 控制命令 ===
on   - 开启LED
off  - 关闭LED
help - 显示帮助
其他 - 回显输入
==================
>> on
LED 已开启
>> off
LED 已关闭
>>
```

## 核心代码解析

### UART初始化流程
```c
// 1. 配置UART参数
uart_config_t uart_config = {
    .baud_rate = 115200,           // 波特率
    .data_bits = UART_DATA_8_BITS, // 8位数据
    .parity = UART_PARITY_DISABLE, // 无校验
    .stop_bits = UART_STOP_BITS_1, // 1位停止
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // 无流控
};

// 2. 安装驱动
uart_driver_install(UART_NUM, rx_buf_size, tx_buf_size, ...);

// 3. 应用配置
uart_param_config(UART_NUM, &uart_config);

// 4. 设置引脚
uart_set_pin(UART_NUM, tx_pin, rx_pin, rts_pin, cts_pin);
```

### 数据收发
```c
// 发送数据
uart_write_bytes(UART_NUM, data, len);

// 接收数据（带超时）
int len = uart_read_bytes(UART_NUM, buf, buf_len, pdMS_TO_TICKS(100));
```

## 进阶挑战

1. **双串口通信**: 使用UART1连接其他模块（如GPS、蓝牙）
2. **不定长数据接收**: 实现帧头帧尾协议解析
3. **DMA传输**: 大数据量时使用DMA提高效率

## 参考资料

- [ESP-IDF UART驱动文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/peripherals/uart.html)
- [UART通信原理](https://www.analog.com/en/analog-dialogue/articles/uart-a-hardware-communication-protocol.html)
