/* WiFi station Example with OLED Display - Level 3-1

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/i2c.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";
static int s_retry_num = 0;

// 保存IP地址用于显示
static char s_ip_address[16] = "0.0.0.0";

/*================== SSD1306 OLED 驱动 ==================*/

// I2C配置
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_SDA_IO       21          // 修改为你的SDA引脚
#define I2C_MASTER_SCL_IO       20          // 修改为你的SCL引脚
#define I2C_MASTER_FREQ_HZ      400000
#define OLED_I2C_ADDRESS        0x3C

// SSD1306命令
#define OLED_CONTROL_BYTE_CMD_SINGLE    0x80
#define OLED_CONTROL_BYTE_CMD_STREAM    0x00
#define OLED_CONTROL_BYTE_DATA_STREAM   0x40
#define OLED_CMD_SET_CONTRAST           0x81
#define OLED_CMD_DISPLAY_OFF            0xAE
#define OLED_CMD_DISPLAY_ON             0xAF
#define OLED_CMD_NORMAL_DISPLAY         0xA6
#define OLED_CMD_INVERT_DISPLAY         0xA7
#define OLED_CMD_SET_MEMORY_ADDR_MODE   0x20
#define OLED_CMD_SET_COLUMN_RANGE       0x21
#define OLED_CMD_SET_PAGE_RANGE         0x22
#define OLED_CMD_SET_PAGE_START_ADDR    0xB0
#define OLED_CMD_SET_COM_SCAN_MODE      0xC8
#define OLED_CMD_SET_DISPLAY_OFFSET     0xD3
#define OLED_CMD_SET_COM_PINS           0xDA
#define OLED_CMD_SET_VCOM_DESELECT      0xDB
#define OLED_CMD_SET_CLOCK_FREQ         0xD5
#define OLED_CMD_SET_PRECHARGE          0xD9
#define OLED_CMD_SET_MULTIPLEX_RATIO    0xA8
#define OLED_CMD_SET_DISPLAY_START_LINE 0x40
#define OLED_CMD_SET_SEGMENT_REMAP      0xA1
#define OLED_CMD_SET_DISPLAY_CLOCK_DIV  0xD5
#define OLED_CMD_CHARGE_PUMP            0x8D
#define OLED_CMD_DISPLAY_RESUME         0xA4

// 字体 (8x6像素字符)
static const uint8_t font6x8[][6] = {
    {0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x00,0x00,0x5F,0x00,0x00,0x00}, // !
    {0x00,0x07,0x00,0x07,0x00,0x00}, // "
    {0x14,0x7F,0x14,0x7F,0x14,0x00}, // #
    {0x24,0x2A,0x7F,0x2A,0x12,0x00}, // $
    {0x23,0x13,0x08,0x64,0x62,0x00}, // %
    {0x36,0x49,0x55,0x22,0x50,0x00}, // &
    {0x00,0x05,0x03,0x00,0x00,0x00}, // '
    {0x00,0x1C,0x22,0x41,0x00,0x00}, // (
    {0x00,0x41,0x22,0x1C,0x00,0x00}, // )
    {0x08,0x2A,0x1C,0x2A,0x08,0x00}, // *
    {0x08,0x08,0x3E,0x08,0x08,0x00}, // +
    {0x00,0x50,0x30,0x00,0x00,0x00}, // ,
    {0x08,0x08,0x08,0x08,0x08,0x00}, // -
    {0x00,0x60,0x60,0x00,0x00,0x00}, // .
    {0x20,0x10,0x08,0x04,0x02,0x00}, // /
    {0x3E,0x51,0x49,0x45,0x3E,0x00}, // 0
    {0x00,0x42,0x7F,0x40,0x00,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46,0x00}, // 2
    {0x21,0x41,0x45,0x4B,0x31,0x00}, // 3
    {0x18,0x14,0x12,0x7F,0x10,0x00}, // 4
    {0x27,0x45,0x45,0x45,0x39,0x00}, // 5
    {0x3C,0x4A,0x49,0x49,0x30,0x00}, // 6
    {0x01,0x71,0x09,0x05,0x03,0x00}, // 7
    {0x36,0x49,0x49,0x49,0x36,0x00}, // 8
    {0x06,0x49,0x49,0x29,0x1E,0x00}, // 9
    {0x00,0x36,0x36,0x00,0x00,0x00}, // :
    {0x00,0x56,0x36,0x00,0x00,0x00}, // ;
    {0x00,0x08,0x14,0x22,0x41,0x00}, // <
    {0x14,0x14,0x14,0x14,0x14,0x00}, // =
    {0x41,0x22,0x14,0x08,0x00,0x00}, // >
    {0x02,0x01,0x51,0x09,0x06,0x00}, // ?
    {0x32,0x49,0x79,0x41,0x3E,0x00}, // @
    {0x7E,0x11,0x11,0x11,0x7E,0x00}, // A
    {0x7F,0x49,0x49,0x49,0x36,0x00}, // B
    {0x3E,0x41,0x41,0x41,0x22,0x00}, // C
    {0x7F,0x41,0x41,0x22,0x1C,0x00}, // D
    {0x7F,0x49,0x49,0x49,0x41,0x00}, // E
    {0x7F,0x09,0x09,0x01,0x01,0x00}, // F
    {0x3E,0x41,0x41,0x51,0x32,0x00}, // G
    {0x7F,0x08,0x08,0x08,0x7F,0x00}, // H
    {0x00,0x41,0x7F,0x41,0x00,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01,0x00}, // J
    {0x7F,0x08,0x14,0x22,0x41,0x00}, // K
    {0x7F,0x40,0x40,0x40,0x40,0x00}, // L
    {0x7F,0x02,0x0C,0x02,0x7F,0x00}, // M
    {0x7F,0x04,0x08,0x10,0x7F,0x00}, // N
    {0x3E,0x41,0x41,0x41,0x3E,0x00}, // O
    {0x7F,0x09,0x09,0x09,0x06,0x00}, // P
    {0x3E,0x41,0x51,0x21,0x5E,0x00}, // Q
    {0x7F,0x09,0x19,0x29,0x46,0x00}, // R
    {0x46,0x49,0x49,0x49,0x31,0x00}, // S
    {0x01,0x01,0x7F,0x01,0x01,0x00}, // T
    {0x3F,0x40,0x40,0x40,0x3F,0x00}, // U
    {0x1F,0x20,0x40,0x20,0x1F,0x00}, // V
    {0x7F,0x20,0x18,0x20,0x7F,0x00}, // W
    {0x63,0x14,0x08,0x14,0x63,0x00}, // X
    {0x03,0x04,0x78,0x04,0x03,0x00}, // Y
    {0x61,0x51,0x49,0x45,0x43,0x00}, // Z
    {0x00,0x00,0x7F,0x41,0x41,0x00}, // [
    {0x02,0x04,0x08,0x10,0x20,0x00}, // /
    {0x41,0x41,0x7F,0x00,0x00,0x00}, // ]
    {0x04,0x02,0x01,0x02,0x04,0x00}, // ^
    {0x40,0x40,0x40,0x40,0x40,0x00}, // _
    {0x00,0x01,0x02,0x04,0x00,0x00}, // `
    {0x20,0x54,0x54,0x54,0x78,0x00}, // a
    {0x7F,0x48,0x44,0x44,0x38,0x00}, // b
    {0x38,0x44,0x44,0x44,0x20,0x00}, // c
    {0x38,0x44,0x44,0x48,0x7F,0x00}, // d
    {0x38,0x54,0x54,0x54,0x18,0x00}, // e
    {0x08,0x7E,0x09,0x01,0x02,0x00}, // f
    {0x08,0x14,0x54,0x54,0x3C,0x00}, // g
    {0x7F,0x08,0x04,0x04,0x78,0x00}, // h
    {0x00,0x44,0x7D,0x40,0x00,0x00}, // i
    {0x20,0x40,0x44,0x3D,0x00,0x00}, // j
    {0x00,0x7F,0x10,0x28,0x44,0x00}, // k
    {0x00,0x41,0x7F,0x40,0x00,0x00}, // l
    {0x7C,0x04,0x18,0x04,0x78,0x00}, // m
    {0x7C,0x08,0x04,0x04,0x78,0x00}, // n
    {0x38,0x44,0x44,0x44,0x38,0x00}, // o
    {0x7C,0x14,0x14,0x14,0x08,0x00}, // p
    {0x08,0x14,0x14,0x18,0x7C,0x00}, // q
    {0x7C,0x08,0x04,0x04,0x08,0x00}, // r
    {0x48,0x54,0x54,0x54,0x20,0x00}, // s
    {0x04,0x3F,0x44,0x40,0x20,0x00}, // t
    {0x3C,0x40,0x40,0x20,0x7C,0x00}, // u
    {0x1C,0x20,0x40,0x20,0x1C,0x00}, // v
    {0x3C,0x40,0x30,0x40,0x3C,0x00}, // w
    {0x44,0x28,0x10,0x28,0x44,0x00}, // x
    {0x0C,0x50,0x50,0x50,0x3C,0x00}, // y
    {0x44,0x64,0x54,0x4C,0x44,0x00}, // z
};

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
    return ESP_OK;
}

static esp_err_t ssd1306_write_cmd(uint8_t cmd)
{
    uint8_t data[2] = {OLED_CONTROL_BYTE_CMD_SINGLE, cmd};
    return i2c_master_write_to_device(I2C_MASTER_NUM, OLED_I2C_ADDRESS, data, 2, 100 / portTICK_PERIOD_MS);
}

static esp_err_t ssd1306_write_data(uint8_t *data, size_t len)
{
    uint8_t *buf = malloc(len + 1);
    if (!buf) return ESP_ERR_NO_MEM;
    buf[0] = OLED_CONTROL_BYTE_DATA_STREAM;
    memcpy(buf + 1, data, len);
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, OLED_I2C_ADDRESS, buf, len + 1, 100 / portTICK_PERIOD_MS);
    free(buf);
    return ret;
}

static void ssd1306_init(void)
{
    vTaskDelay(100 / portTICK_PERIOD_MS);
    ssd1306_write_cmd(OLED_CMD_DISPLAY_OFF);
    ssd1306_write_cmd(OLED_CMD_SET_DISPLAY_CLOCK_DIV);
    ssd1306_write_cmd(0x80);
    ssd1306_write_cmd(OLED_CMD_SET_MULTIPLEX_RATIO);
    ssd1306_write_cmd(0x3F);
    ssd1306_write_cmd(OLED_CMD_SET_DISPLAY_OFFSET);
    ssd1306_write_cmd(0x00);
    ssd1306_write_cmd(OLED_CMD_SET_DISPLAY_START_LINE);
    ssd1306_write_cmd(OLED_CMD_CHARGE_PUMP);
    ssd1306_write_cmd(0x14);
    ssd1306_write_cmd(OLED_CMD_SET_MEMORY_ADDR_MODE);
    ssd1306_write_cmd(0x00);
    ssd1306_write_cmd(OLED_CMD_SET_COM_PINS);
    ssd1306_write_cmd(0x12);
    ssd1306_write_cmd(OLED_CMD_SET_CONTRAST);
    ssd1306_write_cmd(0xCF);
    ssd1306_write_cmd(OLED_CMD_SET_PRECHARGE);
    ssd1306_write_cmd(0xF1);
    ssd1306_write_cmd(OLED_CMD_SET_VCOM_DESELECT);
    ssd1306_write_cmd(0x40);
    ssd1306_write_cmd(OLED_CMD_DISPLAY_RESUME);
    ssd1306_write_cmd(OLED_CMD_NORMAL_DISPLAY);
    ssd1306_write_cmd(OLED_CMD_DISPLAY_ON);
}

static void ssd1306_clear(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        ssd1306_write_cmd(OLED_CMD_SET_PAGE_START_ADDR + page);
        ssd1306_write_cmd(0x00);
        ssd1306_write_cmd(0x10);
        uint8_t data[128] = {0};
        ssd1306_write_data(data, 128);
    }
}

static void ssd1306_draw_char(uint8_t x, uint8_t y, char c)
{
    if (c < 32 || c > 127) c = ' ';
    ssd1306_write_cmd(OLED_CMD_SET_PAGE_START_ADDR + y);
    ssd1306_write_cmd(0x00 + (x & 0x0F));
    ssd1306_write_cmd(0x10 + ((x >> 4) & 0x0F));
    ssd1306_write_data((uint8_t *)font6x8[c - 32], 6);
}

static void ssd1306_draw_string(uint8_t x, uint8_t y, const char *str)
{
    while (*str) {
        ssd1306_draw_char(x, y, *str++);
        x += 6;
        if (x > 122) break;
    }
}

static void ssd1306_display_ip(const char *ip)
{
    ssd1306_clear();
    ssd1306_draw_string(0, 0, "WiFi Connected!");
    ssd1306_draw_string(0, 3, "IP Address:");
    ssd1306_draw_string(0, 5, ip);
}

/*================== WiFi 功能 ==================*/

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        sprintf(s_ip_address, IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "got ip:%s", s_ip_address);
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void app_main(void)
{
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化I2C和OLED
    ESP_LOGI(TAG, "Initializing OLED...");
    ESP_ERROR_CHECK(i2c_master_init());
    ssd1306_init();
    ssd1306_clear();
    ssd1306_draw_string(0, 0, "WiFi Connecting...");
    ssd1306_draw_string(0, 2, "SSID:");
    ssd1306_draw_string(0, 3, EXAMPLE_ESP_WIFI_SSID);

    // 连接WiFi
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    // 显示IP地址
    ssd1306_display_ip(s_ip_address);
    ESP_LOGI(TAG, "IP displayed on OLED: %s", s_ip_address);
}
