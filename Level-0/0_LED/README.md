# ESP32 LED 闪烁项目

这是一个基于 ESP-IDF 框架的 ESP32 LED 闪烁示例项目。

## 功能描述

该项目演示了如何使用 GPIO 控制 LED 闪烁。LED 连接到 GPIO 38，每秒钟切换一次状态（亮/灭）。

## 项目结构

```
.
├── CMakeLists.txt          # 项目级 CMake 配置文件
├── main/
│   ├── CMakeLists.txt      # 组件级 CMake 配置文件
│   └── main.c              # 主程序源代码
├── sdkconfig               # ESP-IDF 配置文件
└── README.md               # 项目说明文档
```

## 代码说明

### 主要功能

- `app_main()`: 程序入口函数，初始化后进入无限循环，每秒切换 LED 状态

### 使用的 GPIO

- **LED_GPIO**: GPIO 38

### 关键 API

- `gpio_set_level()`: 设置 GPIO 电平
- `gpio_get_level()`: 获取 GPIO 当前电平
- `vTaskDelay()`: FreeRTOS 任务延时函数

## 硬件要求

- ESP32-S3 开发板（或其他 ESP32 系列芯片）
- LED（连接到 GPIO 38）

## 编译和烧录

### 环境要求

- ESP-IDF 开发环境（v5.0 或更高版本）

### 编译步骤

1. 设置 ESP-IDF 环境变量：
```bash
. $HOME/esp/esp-idf/export.sh
```

2. 配置项目（可选）：
```bash
idf.py menuconfig
```

3. 编译项目：
```bash
idf.py build
```

4. 烧录固件：
```bash
idf.py flash
```

5. 查看串口输出：
```bash
idf.py monitor
```

或使用组合命令：
```bash
idf.py build flash monitor
```

## 代码亮点

该项目展示了一种简洁的 LED 切换方式：

```c
gpio_set_level(LED_GPIO, gpio_get_level(LED_GPIO) ^ 1);
```

使用异或操作 (`^ 1`) 直接翻转当前电平状态，比传统的先设置高电平再设置低电平更简洁高效。

## 许可证

本项目仅供学习参考使用。
