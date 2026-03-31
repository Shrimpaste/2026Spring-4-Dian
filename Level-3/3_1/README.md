# WiFi Station Example - Level 3-1

这是一个基于ESP-IDF的WiFi Station（STA）模式示例项目。演示如何让ESP32连接到现有的WiFi接入点（AP）。

## 功能特性

- **STA模式**：ESP32作为WiFi客户端连接路由器/AP
- **自动重连**：连接失败时自动重试（可配置重试次数）
- **事件驱动**：使用ESP-IDF事件机制处理WiFi状态变化
- **IP获取通知**：成功连接后显示分配的IP地址

## 项目结构

```
├── CMakeLists.txt          # 项目级CMake配置
├── sdkconfig               # 项目配置（自动生成）
├── main/
│   ├── CMakeLists.txt      # main组件CMake配置
│   ├── sta_main.c          # 主程序源码
│   └── Kconfig.projbuild   # 项目配置选项定义
└── build/                  # 构建输出目录
```

## 环境要求

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) v4.0 或更高版本
- ESP32 开发板
- 可用的WiFi网络

## 快速开始

### 1. 配置WiFi信息

通过menuconfig配置WiFi连接参数：

```bash
idf.py menuconfig
```

进入菜单：`Example Configuration`

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| WiFi SSID | WiFi网络名称 | myssid |
| WiFi Password | WiFi密码 | mypassword |
| Maximum retry | 最大重试次数 | 5 |

**或者在项目根目录创建 `sdkconfig.defaults` 文件：**

```
CONFIG_ESP_WIFI_SSID="your_wifi_ssid"
CONFIG_ESP_WIFI_PASSWORD="your_wifi_password"
CONFIG_ESP_MAXIMUM_RETRY=5
```

### 2. 构建项目

```bash
idf.py build
```

### 3. 烧录到设备

```bash
# 替换 /dev/ttyUSB0 为你的实际串口
idf.py -p /dev/ttyUSB0 flash
```

### 4. 查看日志

```bash
idf.py -p /dev/ttyUSB0 monitor
```

## 编译/烧录/监控（一步完成）

```bash
idf.py -p /dev/ttyUSB0 build flash monitor
```

**退出监控**：按 `Ctrl+]`

## 预期输出

成功连接后，串口将显示类似以下日志：

```
I (0) cpu_start: Starting scheduler on APP CPU.
I (main) wifi station: ESP_WIFI_MODE_STA
I (main) wifi station: wifi_init_sta finished.
I (wifi) wifi station: retry to connect to the AP
I (wifi) wifi station: connected to ap SSID:your_ssid password:your_password
I (wifi) wifi station: got ip:192.168.1.100
```

## 代码说明

### 核心流程

```
app_main()
  └── nvs_flash_init()      # 初始化NVS存储
  └── wifi_init_sta()       # 初始化WiFi Station
      ├── esp_netif_init()  # 初始化网络接口
      ├── esp_wifi_init()   # 初始化WiFi驱动
      ├── esp_wifi_set_config()  # 配置WiFi参数
      └── esp_wifi_start()  # 启动WiFi连接
```

### 事件处理

| 事件 | 处理 |
|------|------|
| `WIFI_EVENT_STA_START` | 开始连接WiFi |
| `WIFI_EVENT_STA_DISCONNECTED` | 重试连接或标记失败 |
| `IP_EVENT_STA_GOT_IP` | 打印获取的IP地址 |

## 故障排除

| 问题 | 解决方法 |
|------|----------|
| 连接失败 | 检查SSID/密码是否正确，确认WiFi信号强度 |
| 无限重启 | 检查电源是否稳定，查看panic日志 |
| 编译错误 | 确保IDF_PATH环境变量正确设置 |
| 烧录失败 | 检查串口权限，确认波特率设置 |

## 许可证

本示例代码采用公有领域（Public Domain / CC0）许可。
