#include "init-4-LED.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void ledinit(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_DIRECTION_OUT);
    gpio_set_level(LED_GPIO, 0);
}