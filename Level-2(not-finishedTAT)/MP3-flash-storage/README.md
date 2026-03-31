# Level-2 MP3播放器 - Flash存储版本

> **⚠️ 当前状态：调试暂停 - 音频输出异常**
>
> ## 已知问题
>
> | 问题 | 状态 | 描述 |
> |------|------|------|
> | 测试音不响 | ❌ 失败 | 1kHz正弦波测试无输出 |
> | MP3播放不响 | ❌ 失败 | 解码正常但无音频输出 |
> | 串口卡死 | ❌ 严重 | 输入指令后系统无响应 |
>
> ## 可能原因分析
>
> 1. **I2S配置问题**
>    - MAX98357A使用Philips I2S格式，但时序可能不匹配
>    - WS信号极性或BCLK相位错误
>    - 采样率配置与MP3文件不匹配（44.1kHz vs 48kHz）
>
> 2. **GPIO/硬件连接问题**
>    - SD引脚(GPIO18)状态不确定，可能为低电平导致功放倒静音
>    - 喇叭连接方式错误（应接OUT+和OUT-，非GND）
>    - MAX98357A供电不足或地线不稳
>
> 3. **任务调度/死锁问题**
>    - 播放器任务与串口任务间的互斥锁死锁
>    - SPI flash缓存访问冲突导致系统卡死
>    - FreeRTOS任务优先级配置不当
>
> 4. **解码器输出问题**
>    - minimp3解码后PCM数据格式不正确
>    - 音量计算导致数据溢出/静音
>    - 立体声/单声道配置与实际数据不匹配
>
> ## 下一步Debug方向
>
> ### 优先级1：硬件验证
> - [ ] 用万用表测量GPIO18(SD引脚)电平，确认3.3V高电平
> - [ ] 检查喇叭接线：直接接OUT+和OUT-（差分输出）
> - [ ] 尝试将SD引脚直接接3.3V（强制使能，最高增益）
> - [ ] 用示波器测量I2S信号：BCLK、WS、DIN是否有波形
>
> ### 优先级2：软件调试
> - [ ] 添加更详细的日志输出（I2S初始化参数、实际采样率）
> - [ ] 使用`esp_rom_gpio_connect_out_signal`直接输出测试波形
> - [ ] 简化播放器任务，移除互斥锁测试是否还卡死
> - [ ] 测试单声道模式（可能立体声配置有问题）
>
> ### 优先级3：替代方案验证
> - [ ] 使用ESP-IDF示例`i2s_std`测试基础I2S输出
> - [ ] 尝试使用旧版I2S驱动API（`driver/i2s.h`而非`driver/i2s_std.h`）
> - [ ] 使用PWM音频输出测试喇叭和功放是否正常
>
> ---

基于ESP32-S3-N8R8开发板和MAX98357AETE音频功放的MP3播放器，使用SPIFFS文件系统存储音频文件。

## 硬件要求

### 开发板
- **ESP32-S3-DevKitC-1** (N8R8 - 8MB Flash, 8MB PSRAM)

### 音频功放模块
- **MAX98357AETE** I2S 3W音频功放模块

### 连接方式

| MAX98357A | ESP32-S3 | 说明 |
|-----------|----------|------|
| LRC/WS    | GPIO5    | 字选择信号 |
| BCLK      | GPIO6    | 位时钟 |
| DIN       | GPIO7    | 数据输入 |
| SD        | GPIO18   | 关断控制（高电平使能） |
| GND       | GND      | 地线 |
| VIN       | 3.3V     | 电源（2.5V-5.5V） |

## 项目结构

```
MP3-flash-storage/
├── CMakeLists.txt              # 主CMake配置
├── sdkconfig.defaults          # 默认SDK配置
├── partitions.csv              # 分区表
├── README.md                   # 本文件
├── flash/                      # Flash数据目录
│   └── data/                   # SPIFFS文件系统内容
│       └── README.txt          # 使用说明
├── components/                 # 外部组件
│   └── minimp3/                # MP3解码器
│       ├── CMakeLists.txt
│       ├── minimp3.h
│       └── minimp3.c
└── main/                       # 主程序
    ├── CMakeLists.txt
    ├── main.c                  # 主入口
    ├── i2s_driver.c            # I2S驱动
    ├── mp3_player.c            # MP3播放器
    └── include/
        ├── i2s_driver.h        # I2S驱动头文件
        └── mp3_player.h        # MP3播放器头文件
```

## 功能特性

- 自动扫描SPIFFS中的MP3文件
- 支持播放/暂停/停止/切换曲目
- 音量控制（0-100%）
- 串口命令控制
- 自动循环播放
- 播放列表管理（最多20首）

## 快速开始

### 1. 添加MP3文件

将MP3文件复制到 `flash/data/` 目录：

```bash
cp /path/to/your/music/*.mp3 /home/sp/Projects/2026Spring-4-Dian/Level-2/MP3-flash-storage/flash/data/
```

### 2. 设置目标芯片

```bash
idf.py set-target esp32s3
```

### 3. 编译并烧录

```bash
idf.py flash monitor
```

### 4. 串口控制

打开串口监视器后，使用以下命令：

| 命令 | 功能 | 状态 |
|------|------|------|
| `p` | 播放/暂停 | ⚠️ 可能卡死 |
| `s` | 停止 | ⚠️ 可能卡死 |
| `n` | 下一首 | ⚠️ 可能卡死 |
| `b` | 上一首 | ⚠️ 可能卡死 |
| `l` | 显示播放列表 | ✅ 正常 |
| `t` | 测试1kHz正弦波 | ❌ 无输出 |
| `v+` | 增加音量 | ⚠️ 可能卡死 |
| `v-` | 减小音量 | ⚠️ 可能卡死 |
| `v80` | 设置音量为80% | ⚠️ 可能卡死 |
| `1-9` | 播放指定曲目 | ⚠️ 可能卡死 |
| `h` | 显示帮助 | ✅ 正常 |

## 分区表说明

```
# Name,   Type, SubType, Offset,  Size,   Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x200000,  # 2MB 应用程序
storage,  data, spiffs,  0x210000,0x5F0000,  # ~6MB SPIFFS存储
```

## 技术细节

### 音频处理流程

1. **SPIFFS文件系统**：MP3文件存储在Flash的SPIFFS分区
2. **minimp3解码**：逐帧解码MP3数据为PCM
3. **I2S输出**：通过I2S接口发送PCM数据到MAX98357A
4. **DAC转换**：MAX98357A将数字音频转换为模拟信号并放大输出

### 支持的音频格式

- **编码**：MPEG Audio Layer III (MP3)
- **采样率**：44.1kHz, 48kHz, 32kHz, 22.05kHz, 24kHz, 16kHz, 11.025kHz, 12kHz, 8kHz
- **声道**：单声道、立体声

## 故障排除

### 没有声音
- 检查MAX98357A的SD引脚是否连接正确（GPIO18）
- 确认MP3文件已正确上传到SPIFFS
- 检查串口输出是否有错误信息

### 编译错误
- 确保ESP-IDF版本 >= 5.0
- 运行 `idf.py fullclean` 后重新编译

### SPIFFS挂载失败
- 首次烧录时会自动格式化SPIFFS
- 检查分区表配置是否正确

## 进阶开发

### 添加更多功能

代码结构清晰，易于扩展：
- `i2s_driver.c/h`：I2S底层驱动，可修改引脚配置
- `mp3_player.c/h`：播放器逻辑，可添加新功能
- `main.c`：主程序入口和命令处理

### 支持更多音频格式

可以集成其他解码器：
- WAV：无需解码，直接播放
- AAC：集成FAAD解码器
- FLAC：集成flac解码器

## 许可证

本项目仅供学习交流使用。

## 参考资料

- [ESP-IDF 官方文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/)
- [minimp3 GitHub](https://github.com/lieff/minimp3)
- [MAX98357A 数据手册](https://www.analog.com/en/products/max98357a.html)
