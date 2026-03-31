# Wi-Fi iperf 测试项目 (Level 3-2)

ESP32 作为 iperf 客户端，PC 作为服务端进行网络性能测量的最小实现。

## 实验要求

- **ESP32 角色**: 客户端 (Client)
- **PC 角色**: 服务端 (Server)
- **测试工具**: iperf2
- **测试目标**: 测量网络带宽 (Bandwidth)

## 硬件接线

| 功能 | 引脚 | 说明 |
|------|------|------|
| UART TX | GPIO43 | 串口输出（波特率 115200） |
| UART RX | GPIO44 | 串口输入 |
| GND | GND | 地线 |
| 3.3V | 3.3V | 电源 |

```
┌─────────────────┐
│   ESP32 开发板   │
│                 │
│  GPIO43 (TX) ───┼───► USB转串口 RX
│  GPIO44 (RX) ───┼───► USB转串口 TX
│  GND      ──────┼───► USB转串口 GND
│  3.3V     ──────┼───► USB转串口 VCC
└─────────────────┘
```

## 配置说明

编辑 `main/iperf_main.c` 文件，修改以下配置：

```c
#define WIFI_SSID           "YOUR_SSID"         // WiFi 名称
#define WIFI_PASSWORD       "YOUR_PASSWORD"     // WiFi 密码
#define IPERF_SERVER_IP     "192.168.1.100"     // PC 服务端 IP 地址
#define IPERF_TEST_TIME     30                  // 测试时长（秒）
```

## 使用步骤

### 1. PC 端启动 iperf 服务端

在 PC 上安装 iperf2（不是 iperf3），然后执行：

```bash
# Windows
iperf.exe -s -i 1

# Linux/Mac
iperf -s -i 1
```

参数说明：
- `-s`: 以服务端模式运行
- `-i 1`: 每秒显示一次报告

### 2. 编译烧录 ESP32

```bash
# 设置目标芯片（以 ESP32-S3 为例）
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录并打开串口监视器
idf.py -p /dev/ttyUSB0 flash monitor
```

### 3. 查看测试结果

ESP32 启动后会自动：
1. 连接 WiFi
2. 向 PC 服务端发起 iperf 测试
3. 输出测试结果（包含 Interval、Transfer、Bandwidth 等字段）

**期望输出示例：**
```
=================================================
|       WiFi iperf Client - Level 3-2       |
=================================================
I (2345) iperf: got ip:192.168.1.50
I (2346) iperf: Starting iperf client test...
I (2346) iperf: Server: 192.168.1.100:5001
I (2347) iperf: Test time: 30 seconds
=================================================
[  3] 0.0- 3.0 sec  3.45 MBytes  9.65 Mbits/sec
[  3] 3.0- 6.0 sec  3.24 MBytes  9.06 Mbits/sec
[  3] 6.0- 9.0 sec  2.96 MBytes  8.27 Mbits/sec
...
```

## 依赖组件

- `espressif/iperf`: ~1.0.2

## 项目结构

```
.
├── CMakeLists.txt          # 项目 CMake 配置
├── sdkconfig.defaults      # 默认配置
├── main/
│   ├── CMakeLists.txt      # main 组件配置
│   ├── idf_component.yml   # 组件依赖
│   └── iperf_main.c        # 主程序（自动连接+测试）
└── README.md               # 本文件
```

## 常见问题

### 1. 找不到 iperf 命令
确保安装的是 **iperf2** 而非 iperf3。iperf2 和 iperf3 协议不兼容。

### 2. 连接超时
- 检查 PC 防火墙是否放行端口 5001
- 确认 PC 和 ESP32 在同一局域网
- 验证 IPERF_SERVER_IP 配置正确

### 3. 速率过低
- 检查 WiFi 信号强度
- 调整测试时长以获得更稳定结果
- 确认 WiFi 信道干扰较少

## 许可证

本示例代码采用公共领域许可（CC0 1.0 Universal）。
