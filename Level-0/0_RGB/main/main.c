#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"

static rmt_channel_handle_t ch;
static rmt_encoder_handle_t enc;
static uint8_t buf[3];

static void rgb(uint8_t r, uint8_t g, uint8_t b) {
    buf[0] = g; buf[1] = r; buf[2] = b;
    rmt_transmit_config_t cfg = {
        .loop_count = 0
    };
    rmt_transmit(ch, enc, buf, 3, &cfg);
    rmt_tx_wait_all_done(ch, -1);
}

void app_main(void) {
    rmt_tx_channel_config_t tx = {
        .clk_src = RMT_CLK_SRC_DEFAULT, 
        .gpio_num = 48,
        .resolution_hz = 10000000,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4
        };
    rmt_new_tx_channel(&tx, &ch);
    led_strip_encoder_config_t ec = {
        .resolution = 10000000
    };
    rmt_new_led_strip_encoder(&ec, &enc);
    rmt_enable(ch);

    uint8_t c[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    while(1) {
        for (int i = 0; i < 3; i++) {
            for (int v = 0; v <= 255; v += 5) {
                rgb(c[i][0] * v / 255, c[i][1] * v / 255, c[i][2] * v / 255);
                vTaskDelay(pdMS_TO_TICKS(30));
            }
            for (int v = 255; v >= 0; v -= 5) {
                rgb(c[i][0] * v / 255, c[i][1] * v / 255, c[i][2] * v / 255);
                vTaskDelay(pdMS_TO_TICKS(30));
            }
        }
    }
}
