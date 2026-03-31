/* Wi-Fi iperf Example - Level 3-2 (最小实现)

   ESP32 作为客户端，PC 作为服务端进行网络性能测量
   This example code is in the Public Domain (or CC0 licensed)
*/

#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "iperf.h"

static const char *TAG = "iperf";

/* ============== 配置区域 ============== */
#define WIFI_SSID           "TEST"         // WiFi 名称
#define WIFI_PASSWORD       "7nnncccom"     // WiFi 密码
#define IPERF_SERVER_IP     "10.204.18.254"     // PC 服务端 IP 地址
#define IPERF_TEST_TIME     30                  // 测试时长（秒）
/* ====================================== */

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static int s_retry_num = 0;
#define ESP_MAXIMUM_RETRY   5

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

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
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

static void iperf_client_task(void *pvParameters)
{
    // 等待 WiFi 连接
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(1000)); // 等待网络稳定

    ESP_LOGI(TAG, "Starting iperf client test...");
    ESP_LOGI(TAG, "Server: %s:%d", IPERF_SERVER_IP, IPERF_DEFAULT_PORT);
    ESP_LOGI(TAG, "Test time: %d seconds", IPERF_TEST_TIME);

    // 解析服务器 IP 地址
    esp_ip_addr_t server_ip;
    ipaddr_aton(IPERF_SERVER_IP, (ip_addr_t *)&server_ip);

    // 使用默认配置创建 iperf 配置
    iperf_cfg_t cfg = IPERF_DEFAULT_CONFIG_CLIENT(IPERF_FLAG_TCP, server_ip);
    cfg.time = IPERF_TEST_TIME;  // 设置测试时长

    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "|    Starting TCP Client Test                   |");
    ESP_LOGI(TAG, "=================================================");

    iperf_start_instance(&cfg);

    // 等待测试完成
    vTaskDelay(pdMS_TO_TICKS((IPERF_TEST_TIME + 5) * 1000));

    ESP_LOGI(TAG, "Test completed!");
    ESP_LOGI(TAG, "\nYou can start a new test by resetting the device.");

    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "=============================================");
    ESP_LOGI(TAG, "|       WiFi iperf Client - Level 3-2       |");
    ESP_LOGI(TAG, "=============================================");
    ESP_LOGI(TAG, "SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "Server IP: %s", IPERF_SERVER_IP);
    ESP_LOGI(TAG, "Test Duration: %d seconds", IPERF_TEST_TIME);
    ESP_LOGI(TAG, "=============================================");

    wifi_init_sta();

    // 创建 iperf 客户端任务
    xTaskCreate(iperf_client_task, "iperf_client", 4096, NULL, 5, NULL);
}
