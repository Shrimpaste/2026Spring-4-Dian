# Level-1 I2S 数字音频合成实验

ESP32-S3通过I2S接口驱动MAX98357A音频功放模块，实现多种波形合成与声道控制。

## 项目结构（模块化封装）

```
main/
├── main.c              # 主程序入口
├── CMakeLists.txt      # 构建配置
│
├── audio_generator.h   # 音频波形生成库
├── audio_generator.c
│   └── 功能：正弦波/锯齿波生成，声道控制
│
├── i2s_driver.h        # I2S驱动封装库
├── i2s_driver.c
│   └── 功能：I2S初始化、音频数据传输
│
├── button_driver.h     # 按键驱动库
├── button_driver.c
│   └── 功能：按键检测、消抖、短按/长按识别
│
└── task_manager.h      # 实验任务管理库
    └── task_manager.c
        └── 功能：三个实验任务的管理与调度
```

## 实验要求

1. **任务1**: 右声道输出500Hz锯齿波，可听见
2. **任务2**: 左声道1001Hz正弦波 + 右声道999Hz正弦波，芯片设置L+R模式，可听见拍频
3. **任务3**: 左声道1kHz正弦波 + 右声道4kHz正弦波，通过BOOT按键切换左/右/L+R声道模式

## 硬件连接

### ESP32-S3 与 MAX98357A

| ESP32-S3 | MAX98357A | 功能说明 |
|----------|-----------|----------|
| GPIO5    | LRC (WS)  | 左右声道选择信号 |
| GPIO6    | BCLK      | 位时钟 |
| GPIO7    | DIN       | 数据输入 |
| 3.3V     | VIN       | 电源 |
| GND      | GND       | 地线 |

### MAX98357A 与喇叭

| MAX98357A | 喇叭 |
|-----------|------|
| SPK+      | +    |
| SPK-      | -    |


## 编译与烧录

```bash
# 设置目标芯片
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录并监控
idf.py -p /dev/ttyUSB0 flash monitor
```

## 使用说明

### 任务切换

| 操作 | 功能 |
|------|------|
| 上电启动 | 自动进入任务1 |
| 短按BOOT键 | 切换到下一个任务 |
| 任务3中短按BOOT | 切换声道模式: 左 → 右 → L+R → 左... |
| 任务3中长按3秒 | 返回任务1 |

### 各任务输出

| 任务 | 左声道 | 右声道 | 芯片模式 | 现象 |
|------|--------|--------|----------|------|
| 1 | 静音 | 500Hz锯齿波 | 立体声 | 听到锯齿波音色 |
| 2 | 1001Hz正弦波 | 999Hz正弦波 | L+R | 听到2Hz拍频(音量周期性起伏) |
| 3-左 | 1kHz正弦波 | 静音 | 左声道 | 1kHz纯音 |
| 3-右 | 静音 | 4kHz正弦波 | 右声道 | 4kHz纯音(较高音调) |
| 3-L+R | 1kHz正弦波 | 4kHz正弦波 | L+R | 同时听到两个频率 |

## 核心代码详解

### 1. I2S初始化流程

```c
// 通道配置
i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
chan_cfg.auto_clear = true;  // 自动清除DMA欠载标志

// 创建通道
i2s_new_channel(&chan_cfg, &tx_chan, NULL);

// 标准I2S配置
i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),  // 44.1kHz采样率
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = { /* GPIO5/6/7配置 */ },
};

// 初始化和启用
i2s_channel_init_std_mode(tx_chan, &std_cfg);
i2s_channel_enable(tx_chan);
```

### 2. 锯齿波生成算法

```c
// 锯齿波: 相位从0到1线性变化，输出-1.0到1.0
float sawtooth = 2.0f * phase - 1.0f;
int16_t sample = (int16_t)(AMPLITUDE * sawtooth);

// 相位增量 = 频率 / 采样率
phase_increment = 2.0f * freq / SAMPLE_RATE;
```

锯齿波特点：
- 波形呈线性上升(或下降)
- 频谱包含基频及其所有整数倍谐波
- 音色明亮、尖锐

### 3. 正弦波生成算法

```c
// y = A * sin(2πft)
float angle = 2.0f * M_PI * phase;
int16_t sample = (int16_t)(AMPLITUDE * sinf(angle));

// 相位范围0-1，增量 = 频率 / 采样率
phase += freq / SAMPLE_RATE;
```

正弦波特点：
- 波形平滑，无谐波
- 纯音，听感干净
- 两个相近频率叠加产生**拍频**现象

### 4. 拍频原理

当两个频率相近的正弦波(f₁=1001Hz, f₂=999Hz)叠加时：

```
y = A·sin(2π·f₁·t) + A·sin(2π·f₂·t)
  = 2A·cos(2π·(f₁-f₂)/2·t)·sin(2π·(f₁+f₂)/2·t)
```

结果：
- 载波频率：(f₁+f₂)/2 = 1000Hz
- 包络频率：(f₁-f₂)/2 = 1Hz
- 拍频(音量起伏)：f₁-f₂ = 2Hz

人耳听到：1000Hz纯音，音量每秒强弱变化2次。

### 5. 按键消抖与长按检测

```c
// 状态机检测
if (gpio_get_level(BOOT_BUTTON_PIN) == 0) {  // 按下
    if (press_start_time == 0) {
        press_start_time = xTaskGetTickCount();
    }
    // 检测长按3秒
    if ((xTaskGetTickCount() - press_start_time) > pdMS_TO_TICKS(3000)) {
        // 长按处理
    }
} else if (press_start_time != 0) {  // 释放
    uint32_t duration = xTaskGetTickCount() - press_start_time;
    if (duration < pdMS_TO_TICKS(3000)) {
        // 短按处理
    }
    press_start_time = 0;
}
```

## 模块流程图

```
┌─────────────────────────────────────────────────────────────┐
│                         app_main()                          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  初始化                                                      
│  ├── boot_button_init()  [配置GPIO0为输入]                   
│  └── i2s_init()          [配置I2S: GPIO5/6/7, 44.1kHz]      
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      主循环 (while 1)                       
│  switch(current_task)                                       
│    ├── case 1: task1_sawtooth_right() ────┐                
│    ├── case 2: task2_beat_frequency() ────┼── 按键切换 ───▶ 
│    └── case 3: task3_channel_switch() ────┘                
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  task1_sawtooth_right()                                     
│  ├── 生成500Hz锯齿波到右声道                                
│  ├── 通过I2S发送音频数据                                    
│  └── 检测BOOT键按下 → 切换到任务2                          
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  task2_beat_frequency()                                     
│  ├── 生成1001Hz正弦波到左声道                               
│  ├── 生成999Hz正弦波到右声道                                
│  ├── 芯片L+R模式 → 合成拍频                                 
│  └── 检测BOOT键按下 → 切换到任务3                          
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  task3_channel_switch()                                     
│  ├── 根据channel_mode生成不同音频:                          
│  │   mode=0: 左1kHz + 右静音                               
│  │   mode=1: 左静音 + 右4kHz                               
│  │   mode=2: 左1kHz + 右4kHz                               
│  ├── 短按BOOT → 切换mode 0→1→2→0...                       
│  └── 长按3秒  → 返回任务1                                   
└─────────────────────────────────────────────────────────────┘
```

## I2S时序说明

```
BCLK:  ─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
        └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─

WS:    ───────┐               ┌───────────────────┐
   (L/RCK)    └───────────────┘                   └────────────
              │←─左声道(L)──→│←────右声道(R)─────→│

DATA:  ──D15─D14─D13─D12─D11─D10─D09─D08─D07─D06─D05─D04─D03─
              │←───────16位左声道数据────────→│
```

- **BCLK**: 位时钟，每个时钟传输1bit数据
- **WS(LRC)**: 字选择信号，低电平=左声道，高电平=右声道
- **DATA**: 串行音频数据，MSB在前

## 关键参数说明

| 参数 | 值 | 说明 |
|------|-----|------|
| SAMPLE_RATE | 44100 | CD音质采样率 |
| BITS_PER_SAMPLE | 16 | 每个样本16bit |
| AMPLITUDE | 8000 | 信号幅度(约满幅度的25%) |
| SAMPLE_COUNT | 512 | 每次生成512个样本帧(立体声=1024个int16) |

## 注意事项

1. **音量控制**: 输出幅度设置为8000(约25%满幅)，避免喇叭过载
2. **非零固定值**: 不要输出非零直流信号，长时间可能损坏喇叭
3. **电源**: MAX98357A可用3.3V或5V供电，5V输出功率更大
4. **地线**: 确保ESP32-S3和MAX98357A共地

## 调试技巧

1. **没有声音**
   - 检查接线：GPIO5/6/7是否对应LRC/BCLK/DIN
   - 确认MAX98357A已上电(VIN有3.3V/5V)
   - 用示波器测量GPIO7是否有波形输出

2. **声音失真**
   - 降低AMPLITUDE值(防止削波)
   - 检查喇叭功率是否足够

3. **听不到拍频**
   - 确认左右声道接线正确
   - 检查喇叭是否能同时还原两个频率

## 模块API详解

### 1. audio_generator - 音频波形生成库

#### 初始化
```c
audio_gen_config_t cfg = {
    .sample_rate = 44100,
    .amplitude = 8000,
};
audio_generator_handle_t gen = audio_gen_create(&cfg);
```

#### 生成正弦波
```c
// 左声道1kHz，右声道4kHz
generate_sine_wave(gen, buffer, 512, 1000, 4000, CHANNEL_INDEPENDENT);
```

#### 声道模式
| 模式 | 说明 |
|------|------|
| CHANNEL_LEFT_ONLY | 仅左声道 |
| CHANNEL_RIGHT_ONLY | 仅右声道 |
| CHANNEL_BOTH | 双声道相同信号 |
| CHANNEL_INDEPENDENT | 左右声道独立频率 |

### 2. i2s_driver - I2S驱动库

#### 初始化
```c
i2s_driver_config_t cfg = {
    .sample_rate = 44100,
    .bits_per_sample = 16,
    .gpio_ws = 5,
    .gpio_bclk = 6,
    .gpio_dout = 7,
    .stereo = true,
};
i2s_driver_handle_t i2s = i2s_driver_init(&cfg);
```

#### 发送音频
```c
// 阻塞式发送，-1表示永久等待
i2s_driver_write(i2s, buffer, sample_count, -1);
```

### 3. button_driver - 按键驱动库

#### 初始化
```c
button_config_t cfg = {
    .gpio_num = GPIO_NUM_0,
    .active_low = true,
    .debounce_ms = 50,
    .long_press_ms = 3000,
};
button_handle_t btn = button_init(&cfg);
```

#### 事件检测
```c
button_event_t event = button_update(btn);
switch (event) {
    case BUTTON_EVENT_SHORT_PRESS:
        // 短按处理
        break;
    case BUTTON_EVENT_LONG_PRESS:
        // 长按处理
        break;
}
```

### 4. task_manager - 任务管理库

#### 创建
```c
task_manager_handle_t tm = task_manager_create(
    i2s, audio_gen, btn, &config
);
```

#### 运行任务
```c
// 在主循环中调用
while (1) {
    task_manager_run(tm);
    vTaskDelay(pdMS_TO_TICKS(1));
}
```

## 参考资料

- [ESP-IDF I2S驱动文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/peripherals/i2s.html)
- [MAX98357A datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf)
- [拍频现象 Wiki](https://en.wikipedia.org/wiki/Beat_(acoustics))
