#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_HIGH_PIN GPIO_NUM_2
#define LED_MED_PIN  GPIO_NUM_4
#define LED_LOW_PIN  GPIO_NUM_5
#define BUTTON_PIN   GPIO_NUM_0

static const char *TAG = "PRIORITY_DEMO";

// ---------------------- Global variables ----------------------
volatile uint32_t high_task_count = 0;
volatile uint32_t med_task_count  = 0;
volatile uint32_t low_task_count  = 0;
volatile bool priority_test_running = false;
volatile bool shared_resource_busy  = false;

// ====================== Step 1: Priority Scheduling ======================
void high_priority_task(void *pvParameters)
{
    ESP_LOGI(TAG, "High Priority Task started (Priority 5)");
    while (1) {
        if (priority_test_running) {
            high_task_count++;
            ESP_LOGI(TAG, "HIGH PRIORITY RUNNING (%d)", high_task_count);
            gpio_set_level(LED_HIGH_PIN, 1);
            for (int i = 0; i < 100000; i++) { volatile int dummy = i * 2; }
            gpio_set_level(LED_HIGH_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        } else vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void medium_priority_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Medium Priority Task started (Priority 3)");
    while (1) {
        if (priority_test_running) {
            med_task_count++;
            ESP_LOGI(TAG, "Medium priority running (%d)", med_task_count);
            gpio_set_level(LED_MED_PIN, 1);
            for (int i = 0; i < 200000; i++) { volatile int dummy = i + 100; }
            gpio_set_level(LED_MED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(300));
        } else vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void low_priority_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Low Priority Task started (Priority 1)");
    while (1) {
        if (priority_test_running) {
            low_task_count++;
            ESP_LOGI(TAG, "Low priority running (%d)", low_task_count);
            gpio_set_level(LED_LOW_PIN, 1);
            for (int i = 0; i < 500000; i++) {
                volatile int dummy = i - 50;
                if (i % 100000 == 0) vTaskDelay(1);
            }
            gpio_set_level(LED_LOW_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        } else vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ====================== Step 2: Round-Robin Tasks ======================
void equal_priority_task1(void *pvParameters)
{
    int id = 1;
    while (1) {
        if (priority_test_running) {
            ESP_LOGI(TAG, "Equal Priority Task %d running", id);
            for (int i = 0; i < 300000; i++) { volatile int dummy = i; }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
void equal_priority_task2(void *pvParameters)
{
    int id = 2;
    while (1) {
        if (priority_test_running) {
            ESP_LOGI(TAG, "Equal Priority Task %d running", id);
            for (int i = 0; i < 300000; i++) { volatile int dummy = i; }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
void equal_priority_task3(void *pvParameters)
{
    int id = 3;
    while (1) {
        if (priority_test_running) {
            ESP_LOGI(TAG, "Equal Priority Task %d running", id);
            for (int i = 0; i < 300000; i++) { volatile int dummy = i; }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ====================== Step 3: Priority Inversion ======================
void priority_inversion_high(void *pvParameters)
{
    while (1) {
        if (priority_test_running) {
            ESP_LOGW(TAG, "High priority task needs shared resource");
            while (shared_resource_busy) {
                ESP_LOGW(TAG, "High priority BLOCKED by low priority!");
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            ESP_LOGI(TAG, "High priority task got resource");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void priority_inversion_low(void *pvParameters)
{
    while (1) {
        if (priority_test_running) {
            ESP_LOGI(TAG, "Low priority task using shared resource");
            shared_resource_busy = true;
            vTaskDelay(pdMS_TO_TICKS(2000));
            shared_resource_busy = false;
            ESP_LOGI(TAG, "Low priority task released resource");
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// ====================== Exercise 1: Dynamic Priority ======================
void dynamic_priority_demo(void *pvParameters)
{
    TaskHandle_t low_task_handle = (TaskHandle_t)pvParameters;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGW(TAG, "Boosting low priority task to priority 4");
        vTaskPrioritySet(low_task_handle, 4);

        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP_LOGW(TAG, "Restoring low priority task to priority 1");
        vTaskPrioritySet(low_task_handle, 1);
    }
}

// ====================== Control Task ======================
void control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Control Task started");
    while (1) {
        if (gpio_get_level(BUTTON_PIN) == 0) {
            if (!priority_test_running) {
                // Step 1
                ESP_LOGW(TAG, "=== STARTING PRIORITY TEST ===");
                priority_test_running = true;
                vTaskDelay(pdMS_TO_TICKS(10000));
                priority_test_running = false;

                // Step 2
                ESP_LOGW(TAG, "=== ROUND-ROBIN TEST ===");
                priority_test_running = true;
                vTaskDelay(pdMS_TO_TICKS(8000));
                priority_test_running = false;

                // Step 3
                ESP_LOGW(TAG, "=== PRIORITY INVERSION TEST ===");
                priority_test_running = true;
                vTaskDelay(pdMS_TO_TICKS(8000));
                priority_test_running = false;

                // Exercise 1
                ESP_LOGW(TAG, "=== DYNAMIC PRIORITY DEMO ===");
                priority_test_running = true;
                vTaskDelay(pdMS_TO_TICKS(10000));
                priority_test_running = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ====================== app_main ======================
void app_main(void)
{
    ESP_LOGI(TAG, "=== FreeRTOS Advanced Scheduling Demo ===");

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_HIGH_PIN) |
                        (1ULL << LED_MED_PIN) |
                        (1ULL << LED_LOW_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    gpio_config_t button_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << BUTTON_PIN,
        .pull_up_en = 1,
        .pull_down_en = 0,
    };
    gpio_config(&button_conf);

    ESP_LOGI(TAG, "Creating tasks on dual cores...");

    // --- Step 1: Priority Scheduling ---
    xTaskCreatePinnedToCore(high_priority_task, "HighPrio", 3072, NULL, 5, NULL, 0); // Core 0
    xTaskCreatePinnedToCore(medium_priority_task, "MedPrio", 3072, NULL, 3, NULL, 0);
    TaskHandle_t low_handle = NULL;
    xTaskCreatePinnedToCore(low_priority_task, "LowPrio", 3072, NULL, 1, &low_handle, 1); // Core 1

    // --- Step 2: Equal priority tasks ---
    xTaskCreate(equal_priority_task1, "Equal1", 2048, NULL, 2, NULL);
    xTaskCreate(equal_priority_task2, "Equal2", 2048, NULL, 2, NULL);
    xTaskCreate(equal_priority_task3, "Equal3", 2048, NULL, 2, NULL);

    // --- Step 3: Priority Inversion ---
    xTaskCreate(priority_inversion_high, "InvHigh", 2048, NULL, 5, NULL);
    xTaskCreate(priority_inversion_low, "InvLow", 2048, NULL, 1, NULL);

    // --- Exercise 1: Dynamic Priority Demo ---
    xTaskCreate(dynamic_priority_demo, "DynamicPrio", 2048, low_handle, 3, NULL);

    // --- Control ---
    xTaskCreate(control_task, "Control", 3072, NULL, 4, NULL);
}
