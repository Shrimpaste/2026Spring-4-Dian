# SD卡SPI读取挂载实验

ESP32通过SPI接口与SD卡通信，实现文件系统挂载和读写测试。

## 实验概要

```
┌─────────────┐                    ┌─────────────┐
│   ESP32S3   │                    │   SD Card   │
│             │      SPI           │             │
│    GPIO4    ├───────────────────►│    MOSI     │
│    (MOSI)   │                    │             │
│             │                    │             │
│    GPIO5    │◄───────────────────┤    MISO     │
│    (MISO)   │                    │             │
│             │                    │             │
│    GPIO2    ├───────────────────►│    SCK      │
│    (CLK)    │                    │             │
│             │                    │             │
│    GPIO8    ├───────────────────►│    CS       │
│    (CS)     │                    │             │
│             │                    │             │
│    3.3V     ├───────────────────►│    VCC      │
│             │                    │             │
│    GND      ├───────────────────►│    GND      │
└─────────────┘                    └─────────────┘
```

## 引脚对应表

| ESP32S3 | SD卡 | 功能说明 | 备注 |
|---------|------|----------|------|
| GPIO4   | MOSI | 主机输出从机输入 | 数据发送 |
| GPIO5   | MISO | 主机输入从机输出 | 数据接收 |
| GPIO2   | SCK  | 时钟信号 | SPI时钟 |
| GPIO8   | CS   | 片选信号 | 低电平有效 |
| 3.3V    | VCC  | 电源 | 必须3.3V，不可接5V |
| GND     | GND  | 地线 | 共地 |

### 其他芯片引脚定义

如需适配其他ESP32芯片，修改代码中的引脚定义：

```c
// ESP32
#define PIN_MOSI  15
#define PIN_MISO  2
#define PIN_CLK   14
#define PIN_CS    13

// ESP32S3 (默认)
#define PIN_MOSI  4
#define PIN_MISO  5
#define PIN_CLK   2
#define PIN_CS    8

// ESP32C3
#define PIN_MOSI  4
#define PIN_MISO  6
#define PIN_CLK   5
#define PIN_CS    1
```

## 硬件连接要求

1. **电平匹配**：SD卡工作电压为3.3V，ESP32 GPIO也是3.3V，可直接连接
2. **上拉电阻**（推荐）：
   - 在MOSI、MISO、SCK、CS线上各加一个10KΩ上拉电阻到3.3V
   - 可提高通信稳定性，特别是使用长杜邦线时

## 使用方法

### 1. 配置项目

```bash
cd /home/sp/Projects/Tests/sdspitest
idf.py set-target esp32s3
```

如需修改引脚或其他配置：
```bash
idf.py menuconfig
# 进入 SD SPI Example Configuration 菜单修改
```

### 2. 编译烧录

```bash
idf.py build
idf.py flash
idf.py monitor
```

### 3. 查看输出

正常启动日志示例：
```
I (0) cpu_start: Starting scheduler on APP CPU.
I (0) SD_CARD_TEST: ========== SD卡SPI挂载实验 ==========
I (0) SD_CARD_TEST: 引脚配置: MOSI=4, MISO=5, CLK=2, CS=8
I (0) SD_CARD_TEST: SPI总线初始化成功
I (0) SD_CARD_TEST: 正在挂载SD卡...
I (0) sdspi_transaction: cmd=52, R1 response: 0x00
I (0) sdspi_transaction: cmd=0, R1 response: 0x01
I (0) vfs_fat_sdmmc: slot 1 (0) mounted to /sdcard
I (0) SD_CARD_TEST: SD卡挂载成功!
I (0) SD_CARD_TEST: Name: SD
I (0) SD_CARD_TEST: Type: SDHC/SDXC
I (0) SD_CARD_TEST: Speed: 20 MHz
I (0) SD_CARD_TEST: Size: 15193 MB
I (0) SD_CARD_TEST: 开始写入测试...
I (0) SD_CARD_TEST: 写入成功: /sdcard/test.txt
I (0) SD_CARD_TEST: 开始读取测试...
I (0) SD_CARD_TEST: 文件内容:
I (0) SD_CARD_TEST:   > Hello SD Card! 你好SD卡！
I (0) SD_CARD_TEST:   > 写入时间戳: 1234
I (0) SD_CARD_TEST: SD卡根目录文件列表:
I (0) SD_CARD_TEST:   test.txt
I (0) SD_CARD_TEST: 正在卸载SD卡...
I (0) SD_CARD_TEST: SD卡已卸载，实验完成!
```

## 故障排查

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 初始化失败 | 引脚连接错误 | 检查MOSI/MISO/CLK/CS连接 |
| 挂载失败 | SD卡格式不支持 | 格式化为FAT32或FAT16 |
| 读取失败 | 信号干扰 | 添加上拉电阻，缩短走线 |
| 无法识别 | SD卡损坏 | 更换SD卡测试 |
| 容量显示异常 | 大容量卡(>32GB) | 确保格式化为FAT32 |

## 实验流程图

```
启动
  │
  ▼
初始化SPI总线
  │
  ▼
挂载FAT文件系统 ──失败──► 报错退出
  │
  ▼
打印SD卡信息
  │
  ▼
写入测试文件
  │
  ▼
读取并显示内容
  │
  ▼
列出根目录文件
  │
  ▼
卸载SD卡
  │
  ▼
结束
```

## 扩展功能

代码中预留了扩展点，可在此基础上添加：
- 文件追加写入模式 (`"a"`)
- 二进制文件读写 (`"rb"`/`"wb"`)
- 目录创建 (`mkdir`)
- 文件删除 (`unlink`)
- 大文件测试
- 读写速度测试
