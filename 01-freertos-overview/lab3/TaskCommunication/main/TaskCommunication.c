#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_PIN GPIO_NUM_2
static const char *TAG = "COMM_TASKS";

// ---------------- Shared Variable ----------------
volatile int shared_counter = 0;

// ---------------- Producer ----------------
void producer_task(void *pvParameters)
{
    while (1) {
        shared_counter++;
        ESP_LOGI(TAG, "Producer: counter = %d", shared_counter);
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(900));  // 1 second total
    }
}

// ---------------- Consumer ----------------
void consumer_task(void *pvParameters)
{
    int last_value = 0;

    while (1) {
        if (shared_counter != last_value) {
            ESP_LOGI(TAG, "Consumer: received %d", shared_counter);
            last_value = shared_counter;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ---------------- Main ----------------
void app_main(void)
{
    ESP_LOGI(TAG, "=== FreeRTOS Communication Demo ===");

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    xTaskCreate(producer_task, "ProducerTask", 2048, NULL, 2, NULL);
    xTaskCreate(consumer_task, "ConsumerTask", 2048, NULL, 1, NULL);
}
