#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

#define LED1_PIN GPIO_NUM_2
#define LED2_PIN GPIO_NUM_4

static const char *TAG = "SELFDELETE_TASKS";

// ---------------- LED Tasks ----------------
void led1_task(void *pvParameters)
{
    while (1) {
        gpio_set_level(LED1_PIN, 1);
        ESP_LOGI(TAG, "LED1 ON");
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(LED1_PIN, 0);
        ESP_LOGI(TAG, "LED1 OFF");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void led2_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "LED2 Blink Fast");
        for (int i = 0; i < 3; i++) {
            gpio_set_level(LED2_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED2_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------------- Self-Deletion Task ----------------
void temporary_task(void *pvParameters)
{
    int *duration = (int *)pvParameters;
    ESP_LOGI(TAG, "Temporary task will run for %d seconds", *duration);

    for (int i = *duration; i > 0; i--) {
        ESP_LOGI(TAG, "Temporary task countdown: %d", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Temporary task self-deleting");
    vTaskDelete(NULL);
}

// ---------------- Main ----------------
void app_main(void)
{
    ESP_LOGI(TAG, "=== FreeRTOS Self-Deletion Demo ===");

    // Configure GPIOs
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED1_PIN) | (1ULL << LED2_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    // Create LED Tasks
    xTaskCreate(led1_task, "LED1_Task", 2048, NULL, 2, NULL);
    xTaskCreate(led2_task, "LED2_Task", 2048, NULL, 2, NULL);

    // Create Temporary Task
    static int temp_duration = 10; // seconds
    xTaskCreate(temporary_task, "TempTask", 2048, &temp_duration, 1, NULL);

    ESP_LOGI(TAG, "All tasks created successfully");
}
