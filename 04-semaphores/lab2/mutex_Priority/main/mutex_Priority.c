#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_random.h"

static const char *TAG = "MUTEX_PRIO";

// LED pins
#define LED_TASK1 GPIO_NUM_2
#define LED_TASK2 GPIO_NUM_4
#define LED_TASK3 GPIO_NUM_5
#define LED_CRITICAL GPIO_NUM_18

SemaphoreHandle_t xMutex;

// Shared data
typedef struct {
    uint32_t counter;
    char shared_buffer[100];
    uint32_t checksum;
    uint32_t access_count;
} shared_resource_t;

shared_resource_t shared_data = {0, "", 0, 0};

// Stats
typedef struct {
    uint32_t successful_access;
    uint32_t failed_access;
    uint32_t corruption_detected;
} access_stats_t;

access_stats_t stats = {0, 0, 0};

// Simple checksum
uint32_t calculate_checksum(const char* data, uint32_t counter) {
    uint32_t sum = counter;
    for (int i = 0; data[i] != '\0'; i++) {
        sum += (uint32_t)data[i] * (i + 1);
    }
    return sum;
}

// Access with Mutex protection
void access_shared_resource(const char* task_name, gpio_num_t led_pin) {
    char temp_buffer[100];
    uint32_t temp_counter;
    uint32_t expected_checksum;

    ESP_LOGI(TAG, "[%s] Requesting access to shared resource...", task_name);

    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        ESP_LOGI(TAG, "[%s] ✓ Mutex acquired", task_name);
        stats.successful_access++;

        gpio_set_level(led_pin, 1);
        gpio_set_level(LED_CRITICAL, 1);

        // Critical section
        temp_counter = shared_data.counter;
        strcpy(temp_buffer, shared_data.shared_buffer);
        expected_checksum = shared_data.checksum;

        uint32_t calculated_checksum = calculate_checksum(temp_buffer, temp_counter);
        if (calculated_checksum != expected_checksum && shared_data.access_count > 0) {
            ESP_LOGE(TAG, "[%s] ⚠️ Data corruption detected!", task_name);
            stats.corruption_detected++;
        }

        vTaskDelay(pdMS_TO_TICKS(500 + (esp_random() % 1000)));

        shared_data.counter = temp_counter + 1;
        snprintf(shared_data.shared_buffer, sizeof(shared_data.shared_buffer),
                 "Modified by %s #%lu", task_name, shared_data.counter);
        shared_data.checksum = calculate_checksum(shared_data.shared_buffer, shared_data.counter);
        shared_data.access_count++;

        ESP_LOGI(TAG, "[%s] Updated -> Counter:%lu, Buffer:'%s'",
                 task_name, shared_data.counter, shared_data.shared_buffer);

        vTaskDelay(pdMS_TO_TICKS(200 + (esp_random() % 500)));

        gpio_set_level(led_pin, 0);
        gpio_set_level(LED_CRITICAL, 0);

        xSemaphoreGive(xMutex);
        ESP_LOGI(TAG, "[%s] Mutex released", task_name);

    } else {
        ESP_LOGW(TAG, "[%s] ✗ Failed to acquire mutex", task_name);
        stats.failed_access++;
    }
}

// High priority task (now lowered)
void high_priority_task(void *pvParameters) {
    ESP_LOGI(TAG, "High Priority Task started (Priority: %d)", uxTaskPriorityGet(NULL));
    while (1) {
        access_shared_resource("HIGH_PRI", LED_TASK1);
        vTaskDelay(pdMS_TO_TICKS(4000 + (esp_random() % 2000)));
    }
}

// Medium priority task
void medium_priority_task(void *pvParameters) {
    ESP_LOGI(TAG, "Medium Priority Task started (Priority: %d)", uxTaskPriorityGet(NULL));
    while (1) {
        access_shared_resource("MED_PRI", LED_TASK2);
        vTaskDelay(pdMS_TO_TICKS(3000 + (esp_random() % 2000)));
    }
}

// Low priority task (now raised)
void low_priority_task(void *pvParameters) {
    ESP_LOGI(TAG, "Low Priority Task started (Priority: %d)", uxTaskPriorityGet(NULL));
    while (1) {
        access_shared_resource("LOW_PRI", LED_TASK3);
        vTaskDelay(pdMS_TO_TICKS(2000 + (esp_random() % 1000)));
    }
}

// Monitor task
void monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "System monitor started");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15000));
        ESP_LOGI(TAG, "\n═══ PRIORITY INVERSION MONITOR ═══");
        ESP_LOGI(TAG, "Mutex Available: %s",
                uxSemaphoreGetCount(xMutex) ? "YES" : "NO (Held)");
        ESP_LOGI(TAG, "Counter: %lu", shared_data.counter);
        ESP_LOGI(TAG, "Buffer: '%s'", shared_data.shared_buffer);
        ESP_LOGI(TAG, "Access Count: %lu", shared_data.access_count);

        uint32_t current_checksum = calculate_checksum(shared_data.shared_buffer, shared_data.counter);
        if (current_checksum != shared_data.checksum && shared_data.access_count > 0) {
            ESP_LOGE(TAG, "⚠️ CURRENT DATA CORRUPTION DETECTED!");
            stats.corruption_detected++;
        }

        ESP_LOGI(TAG, "Stats -> Success:%lu, Failed:%lu, Corrupted:%lu",
                 stats.successful_access, stats.failed_access, stats.corruption_detected);
        ESP_LOGI(TAG, "══════════════════════════════\n");
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Experiment 3: Priority Adjustment Starting...");

    gpio_set_direction(LED_TASK1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_TASK2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_TASK3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_CRITICAL, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_TASK1, 0);
    gpio_set_level(LED_TASK2, 0);
    gpio_set_level(LED_TASK3, 0);
    gpio_set_level(LED_CRITICAL, 0);

    // Create Mutex
    xMutex = xSemaphoreCreateMutex();

    if (xMutex != NULL) {
        ESP_LOGI(TAG, "Mutex created successfully");

        shared_data.counter = 0;
        strcpy(shared_data.shared_buffer, "Initial state");
        shared_data.checksum = calculate_checksum(shared_data.shared_buffer, shared_data.counter);

        // 🔽 Changed priorities (High ↓, Low ↑)
        xTaskCreate(high_priority_task, "HighPri", 3072, NULL, 2, NULL); // Lowered
        xTaskCreate(medium_priority_task, "MedPri", 3072, NULL, 3, NULL);
        xTaskCreate(low_priority_task, "LowPri", 3072, NULL, 5, NULL);   // Raised
        xTaskCreate(monitor_task, "Monitor", 3072, NULL, 1, NULL);

        ESP_LOGI(TAG, "All tasks created with modified priorities:");
        ESP_LOGI(TAG, "  High Priority Task: 2");
        ESP_LOGI(TAG, "  Medium Priority:    3");
        ESP_LOGI(TAG, "  Low Priority:       5");
        ESP_LOGI(TAG, "  Monitor Task:       1");
        ESP_LOGI(TAG, "\nWatch how LOW_PRI now preempts others (Priority Inversion Demo)");

    } else {
        ESP_LOGE(TAG, "Failed to create mutex!");
    }
}
