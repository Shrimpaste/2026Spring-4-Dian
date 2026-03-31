#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "init-4-LED.h"
#define LED_GPIO 38

void app_main(void)
{
    ledinit();

    while (1) {
        /*gpio_set_level(LED_GPIO, 1);                                                                                                                                      
        gpio_set_level(LED_GPIO, level);                                                          
        vTaskDelay(1000);
        gpio_set_level(LED_GPIO, 0);                                                              
        vTaskDelay(1000);*/ //normal way to toggle                                                                      
        gpio_set_level(LED_GPIO, gpio_get_level(LED_GPIO) ^ 1); // Toggle                         
        //vTaskDelay(1000); // Delay for 1 second，需要配置1000
        vTaskDelay(pdMS_TO_TICKS(1000)); //1s转为时钟对应数值，无需配置1000                                                   
        //more cool 
    }
}