# ESP32-S3 WiFi AP+STA NAT 路由器

## 项目简介

本项目基于 ESP32-S3 实现了一个迷你 WiFi 路由器功能，支持 AP+STA 双模式同时运行：

- **STA 模式**: ESP32-S3 作为客户端连接到现有 WiFi 路由器（上级网络）
- **AP 模式**: ESP32-S3 创建 WiFi 热点，供其他设备接入
- **NAPT**: 启用网络地址端口转换，使连接到 AP 的设备可以通过 STA 访问互联网

## 硬件接线

本项目仅需 ESP32-S3 开发板，无需额外外设。

### 基本接线

| 功能 | 接线说明 |
|------|----------|
| 供电 | USB Type-C 数据线连接电脑或电源适配器（5V） |
| 烧录/调试 | 通过 USB 直接连接（ESP32-S3 内置 USB-Serial/JTAG） |

### 开发板引脚参考（ESP32-S3-DevKitC）

```
+------------------+
|  EN/RESET  [ ]   |  ← 复位按钮
|      GPIO0 [ ]   |  ← Boot 按钮（按住+复位进入下载模式）
|      GPIO1 [ ]   |
|      GPIO2 [ ]   |
|      ...   [ ]   |
| USB Port (Type-C)|  ← 烧录和供电
+------------------+
```

## 软件配置

### 环境要求

- ESP-IDF v5.0 或更高版本
- Python 3.7+

### 配置 WiFi 参数

使用 menuconfig 配置网络参数：

```bash
idf.py menuconfig
```

进入 `Example Configuration` 菜单配置以下参数：

#### SoftAP 配置（本机热点）

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| WiFi AP SSID | `myssid` | 创建的热点名称 |
| WiFi AP Password | `mypassword` | 热点密码（至少8位） |
| WiFi AP Channel | `1` | 信道 1-14 |
| Maximal STA connections | `4` | 最大接入设备数 |

#### STA 配置（上级路由器）

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| WiFi Remote AP SSID | `otherapssid` | 要连接的路由器名称 |
| WiFi Remote AP Password | `otherappassword` | 路由器密码 |
| Maximum retry | `5` | 连接失败重试次数 |
| WiFi Scan auth mode | `WPA2 PSK` | 认证模式 |

### 关键配置说明

- **上级路由器**：STA 模式下要连接的 WiFi 网络（需要有互联网访问）
- **本机热点**：AP 模式下创建的网络，其他设备连接此热点上网
- **IP 分配**：
  - 上级路由器会给 ESP32-S3 分配 IP（如 192.168.1.x）
  - ESP32-S3 会给接入设备分配 IP（默认 192.168.4.x）

## 编译与烧录

### 1. 设置环境变量

```bash
get_idf  # 或 source $IDF_PATH/export.sh
```

### 2. 编译项目

```bash
idf.py build
```

### 3. 烧录固件

```bash
# 自动检测端口烧录
idf.py flash

# 或指定端口
idf.py -p /dev/ttyACM0 flash
```

> ESP32-S3 按住 BOOT 键同时按 RESET 键进入下载模式

### 4. 查看日志

```bash
idf.py monitor

# 或指定端口
idf.py -p /dev/ttyACM0 monitor
```

按 `Ctrl+]` 退出 monitor。

## 使用方式

### 正常启动流程

1. **上电启动**：ESP32-S3 自动开始运行
2. **连接上级网络**：STA 模式尝试连接配置的远程 AP
3. **创建热点**：AP 模式启动，创建 WiFi 热点
4. **启用 NAT**：连接成功后自动启用 NAPT 转发

### 查看运行状态

通过串口 monitor 可以看到以下日志：

```
I (1234) WiFi Sta: Station started
I (2345) WiFi Sta: Got IP:192.168.1.100
I (2346) WiFi SoftAP: wifi_init_softap finished. SSID:myssid password:mypassword channel:1
I (3456) WiFi SoftAP: Station 12:34:56:78:9A:BC joined, AID=1
I (3457) WiFi SoftAP: Assigned IP to client: 192.168.4.2, MAC=12:34:56:78:9A:BC, hostname='MyPhone'
```

### 客户端连接

1. 手机/电脑搜索 WiFi，连接到 `myssid`（或你配置的 SSID）
2. 输入密码连接
3. 连接成功后即可通过 ESP32-S3 访问互联网

### 指示灯说明

| 状态 | 说明 |
|------|------|
| 持续快闪 | 正在连接上级 WiFi |
| 慢闪 | 已连接上级，等待客户端接入 |
| 常亮 | 正常工作，有客户端连接 |

## 故障排查

| 问题 | 可能原因 | 解决方法 |
|------|----------|----------|
| 无法连接上级 WiFi | SSID/密码错误 | 检查 menuconfig 配置 |
| 客户端无法上网 | NAPT 未启用 | 检查 sdkconfig 中 `CONFIG_LWIP_IPV4_NAPT=y` |
| 烧录失败 | 未进入下载模式 | 按住 BOOT，按 RESET，松开 BOOT |
| 频繁断线 | 信号弱/电源不稳 | 检查供电，靠近路由器测试 |

## 项目结构

```
.
├── CMakeLists.txt          # 项目 CMake 配置
├── sdkconfig.defaults      # 默认 sdkconfig（启用 IP_FORWARD 和 NAPT）
├── main/
│   ├── CMakeLists.txt      # main 组件配置
│   ├── Kconfig.projbuild   # 项目配置菜单定义
│   └── apsta_nat_main.c    # 主程序代码
└── README.md               # 本文件
```

## 注意事项

1. **AP 密码长度**：必须至少 8 个字符，否则会自动变为开放网络
2. **信道选择**：建议选择与上级路由器相同或相邻信道，减少干扰
3. **性能限制**：ESP32-S3 的 WiFi 吞吐量有限，适合轻量级上网需求
4. **功耗考虑**：双模式运行功耗较高，建议使用稳定的电源供电

## 许可证

基于 CC0-1.0 / Unlicense 开源协议
