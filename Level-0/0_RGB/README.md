# ESP32 WS2812 RGB呼吸灯

ESP32-S3使用RMT驱动WS2812 RGB LED，实现**红、绿、蓝三色循环呼吸**效果。

## 效果说明

- **颜色顺序**：红色 → 绿色 → 蓝色 → 循环
- **呼吸效果**：每种颜色经历 暗→亮→暗 的变化
- **亮度控制**：0~255线性渐变，每次变化5，步进30ms
- **周期**：约3秒完成一种颜色呼吸，9秒完成一轮三色循环

## 硬件连接

| 信号 | GPIO | 说明 |
|------|------|------|
| 数据线 | GPIO48 | ESP32-S3开发板板载WS2812默认引脚 |

如GPIO48不工作，请修改为实际连接的GPIO（常见：GPIO8, GPIO18等）。

## 核心代码逻辑

```c
// 三种基础颜色：红、绿、蓝
uint8_t c[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};

for (;;) {
    for (int i = 0; i < 3; i++) {           // 遍历三种颜色
        for (int v = 0; v <= 255; v += 5) {  // 亮度渐增
            rgb(c[i][0] * v / 255, c[i][1] * v / 255, c[i][2] * v / 255);
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        for (int v = 255; v >= 0; v -= 5) {  // 亮度渐减
            rgb(c[i][0] * v / 255, c[i][1] * v / 255, c[i][2] * v / 255);
            vTaskDelay(pdMS_TO_TICKS(30));
        }
    }
}
```

## 技术要点

### RMT配置

```c
rmt_tx_channel_config_t tx = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .gpio_num = 48,
    .resolution_hz = 10000000,  // 10MHz = 100ns分辨率
    .mem_block_symbols = 64,
    .trans_queue_depth = 4
};
```

- **10MHz分辨率**：满足WS2812时序要求（T0H=0.3μs, T1H=0.9μs）
- **自定义编码器**：将RGB字节转换为WS2812归零码时序

### WS2812协议

| 比特 | 高电平 | 低电平 |
|------|--------|--------|
| 0 | 0.3μs | 0.9μs |
| 1 | 0.9μs | 0.3μs |
| Reset | >50μs 低电平 | - |

### 数据格式

WS2812使用**GRB顺序**（非RGB）：
```c
buf[0] = g;  // 绿色
buf[1] = r;  // 红色
buf[2] = b;  // 蓝色
```

## 编译与烧录

```bash
# 设置ESP-IDF环境
. $HOME/esp/esp-idf/export.sh

# 配置目标芯片（本项目使用esp32s3）
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录并监控
idf.py -p /dev/ttyUSB0 flash monitor
```

## 修改参数

### 修改GPIO

```c
rmt_tx_channel_config_t tx = {
    .gpio_num = 48,   // 修改为实际使用的GPIO
    ...
};
```

### 调整呼吸速度

```c
vTaskDelay(pdMS_TO_TICKS(30));  // 增大数值变慢，减小变快
```

### 修改亮度步进

```c
for (int v = 0; v <= 255; v += 5)  // 步进5，改为10则变化更快
```

### 自定义颜色

```c
// 添加更多颜色或修改现有颜色
uint8_t c[][3] = {
    {255, 0, 0},    // 红
    {255, 128, 0},  // 橙
    {255, 255, 0},  // 黄
    {0, 255, 0},    // 绿
    {0, 0, 255},    // 蓝
    {128, 0, 255}   // 紫
};
```

### 扩展到多灯珠

```c
#define NUM_LEDS 8
static uint8_t buf[NUM_LEDS * 3];  // 8颗灯珠

// 填充数据后发送全部
rmt_transmit(ch, enc, buf, NUM_LEDS * 3, &cfg);
```

## 文件结构

```
main/
├── main.c              // 主程序：呼吸灯逻辑
├── led_strip_encoder.c // RMT编码器：字节→WS2812时序
└── led_strip_encoder.h // 编码器头文件
```

## led-strip-encoder 说明

`led-strip-encoder` 是 ESP-IDF 框架下的 RMT 编码器，专门用于驱动 LED 灯带（如 WS2812）。

### 核心作用

1. **将像素数据编码为 RMT 符号**
   - 将 RGB 像素数据转换为 ESP32 的 RMT (Remote Control) 外设可识别的符号
   - 根据 WS2812 时序要求，将每个 bit 编码为高/低电平的组合

2. **WS2812 时序编码**

   | 比特 | 高电平 | 低电平 |
   |------|--------|--------|
   | 0 | 0.3μs | 0.9μs |
   | 1 | 0.9μs | 0.3μs |

3. **双状态编码流程**

   | 状态 | 操作 |
   |------|------|
   | **State 0** | 发送 RGB 数据（通过 bytes_encoder） |
   | **State 1** | 发送 RESET 信号（50μs 低电平） |

### 编码器结构

```c
typedef struct {
    rmt_encoder_t base;           // RMT 编码器基类
    rmt_encoder_t *bytes_encoder; // 字节编码器，处理 RGB 数据
    rmt_encoder_t *copy_encoder;  // 复制编码器，发送 RESET 信号
    int state;                    // 当前编码状态
    rmt_symbol_word_t reset_code; // 复位码配置
} rmt_led_strip_encoder_t;
```

### 配置参数

```c
led_strip_encoder_config_t config = {
    .resolution = 10000000,  // 编码器分辨率，单位 Hz
};
```

- **分辨率**：决定时序精度，10MHz = 100ns 时间分辨率
- **传输顺序**：MSB First（高位在前）
- **颜色顺序**：G7...G0 → R7...R0 → B7...B0（WS2812 使用 GRB 顺序）

### 主要接口

```c
// 创建编码器
esp_err_t rmt_new_led_strip_encoder(
    const led_strip_encoder_config_t *config,
    rmt_encoder_handle_t *ret_encoder
);

// 编码函数（内部调用）
size_t rmt_encode_led_strip(
    rmt_encoder_t *encoder,
    rmt_channel_handle_t channel,
    const void *primary_data,
    size_t data_size,
    rmt_encode_state_t *ret_state
);

// 删除编码器
esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder);

// 重置编码器
esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder);
```

### 工作流程

```
RGB数据输入 → bytes_encoder编码 → RMT符号 → 发送给WS2812
                    ↓
            数据发送完成 → copy_encoder发送50μs RESET码 → 结束
```

这是 ESP32 驱动 WS2812/SK6812 等 LED 灯带的标准组件，通过 RMT 硬件实现精确的时序控制，无需软件 bit-banging，节省 CPU 资源。

## 参考资料

- [WS2812 数据手册](https://cdn-shop.adafruit.com/datasheets/WS2812.pdf)
- [ESP-IDF RMT驱动文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/rmt.html)
