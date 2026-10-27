#include "esp_heap_caps.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
int x4_application_main(void);
void app_main(void)
{
    printf("X4: reset=%d wake=%d\n",esp_reset_reason(),esp_sleep_get_wakeup_cause());
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
        gpio_config_t power = { .pin_bit_mask=1ULL<<3, .mode=GPIO_MODE_INPUT, .pull_up_en=GPIO_PULLUP_ENABLE };
        gpio_config(&power);
        while (!gpio_get_level(GPIO_NUM_3)) vTaskDelay(pdMS_TO_TICKS(20));
    }
    printf("X4: boot heap=%zu largest=%zu\n", (size_t)esp_get_free_heap_size(),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    int result = x4_application_main();
    printf("X4: application exit=%d heap=%zu minimum=%zu\n", result,
        (size_t)esp_get_free_heap_size(), (size_t)esp_get_minimum_free_heap_size());
    if (result != 0) {
        printf("X4: application failed; USB stays available for recovery\n");
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    while (!gpio_get_level(GPIO_NUM_3)) vTaskDelay(pdMS_TO_TICKS(20));
    esp_deep_sleep_enable_gpio_wakeup(1ULL << 3, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
}
