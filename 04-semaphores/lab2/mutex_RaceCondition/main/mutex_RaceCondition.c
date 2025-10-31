#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_random.h"

static const char *TAG = "MUTEX_OFF";

// LED pins for different tasks
#define LED_TASK1 GPIO_NUM_2
#define LED_TASK2 GPIO_NUM_4
#define LED_TASK3 GPIO_NUM_5
#define LED_CRITICAL GPIO_NUM_18

SemaphoreHandle_t xMutex;

typedef struct {
    uint32_t counter;
    char shared_buffer[100];
    uint32_t checksum;
    uint32_t access_count;
} shared_resource_t;

shared_resource_t shared_data = {0, "", 0, 0};

typedef struct {
    uint32_t successful_access;
    uint32_t failed_access;
    uint32_t corruption_detected;
} access_stats_t;

access_stats_t stats = {0, 0, 0};

uint32_t calculate_checksum(const char* data, uint32_t counter) {
    uint32_t sum = counter;
    for (int i = 0; data[i] != '\0'; i++) {
        sum += (uint32_t)data[i] * (i + 1);
    }
    return sum;
}

// ⚠️ Mutex removed — this will cause race condition
void access_shared_resource(int task_id, const char* task_name, gpio_num_t led_pin) {
    char temp_buffer[100];
    uint32_t temp_counter;
    uint32_t expected_checksum;

    ESP_LOGI(TAG, "[%s] Accessing shared resource (NO MUTEX!)", task_name);

    // Turn on LEDs — simulate entering critical section
    gpio_set_level(led_pin, 1);
    gpio_set_level(LED_CRITICAL, 1);

    // === NO MUTEX PROTECTION BEGIN ===
    temp_counter = shared_data.counter;
    strcpy(temp_buffer, shared_data.shared_buffer);
    expected_checksum = shared_data.checksum;

    uint32_t calculated_checksum = calculate_checksum(temp_buffer, temp_counter);
    if (calculated_checksum != expected_checksum && shared_data.access_count > 0) {
        ESP_LOGE(TAG, "[%s] ⚠️ DATA CORRUPTION DETECTED!", task_name);
        stats.corruption_detected++;
    }

    vTaskDelay(pdMS_TO_TICKS(500 + (esp_random() % 1000)));

    shared_data.counter = temp_counter + 1;
    snprintf(shared_data.shared_buffer, sizeof(shared_data.shared_buffer),
             "Modified by %s #%lu", task_name, shared_data.counter);
    shared_data.checksum = calculate_checksum(shared_data.shared_buffer, shared_data.counter);
    shared_data.access_count++;

    ESP_LOGI(TAG, "[%s] Modified -> Counter:%lu Buffer:'%s'",
             task_name, shared_data.counter, shared_data.shared_buffer);

    vTaskDelay(pdMS_TO_TICKS(200 + (esp_random() % 500)));
    // === NO MUTEX PROTECTION END ===

    gpio_set_level(led_pin, 0);
    gpio_set_level(LED_CRITICAL, 0);

    stats.successful_access++;
}

void high_priority_task(void *pvParameters) {
    ESP_LOGI(TAG, "High Priority Task started (Prio %d)", uxTaskPriorityGet(NULL));
    while (1) {
        access_shared_resource(1, "HIGH_PRI", LED_TASK1);
        vTaskDelay(pdMS_TO_TICKS(5000 + (esp_random() % 3000)));
    }
}

void medium_priority_task(void *pvParameters) {
    ESP_LOGI(TAG, "Medium Priority Task started (Prio %d)", uxTaskPriorityGet(NULL));
    while (1) {
        access_shared_resource(2, "MED_PRI", LED_TASK2);
        vTaskDelay(pdMS_TO_TICKS(3000 + (esp_random() % 2000)));
    }
}

void low_priority_task(void *pvParameters) {
    ESP_LOGI(TAG, "Low Priority Task started (Prio %d)", uxTaskPriorityGet(NULL));
    while (1) {
        access_shared_resource(3, "LOW_PRI", LED_TASK3);
        vTaskDelay(pdMS_TO_TICKS(2000 + (esp_random() % 1000)));
    }
}

void monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "System monitor started");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15000));
        ESP_LOGI(TAG, "\n═══ RACE CONDITION MONITOR ═══");
        ESP_LOGI(TAG, "Counter: %lu", shared_data.counter);
        ESP_LOGI(TAG, "Buffer: '%s'", shared_data.shared_buffer);
        ESP_LOGI(TAG, "Checksum: %lu", shared_data.checksum);
        ESP_LOGI(TAG, "Access Count: %lu", shared_data.access_count);

        uint32_t current_checksum = calculate_checksum(shared_data.shared_buffer, shared_data.counter);
        if (current_checksum != shared_data.checksum && shared_data.access_count > 0) {
            ESP_LOGE(TAG, "⚠️ CURRENT DATA CORRUPTION DETECTED!");
            stats.corruption_detected++;
        }

        ESP_LOGI(TAG, "Stats: Success:%lu  Corrupted:%lu",
                 stats.successful_access, stats.corruption_detected);
        ESP_LOGI(TAG, "══════════════════════════════\n");
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "⚠️ Mutex Disabled - Race Condition Demo Starting...");

    gpio_set_direction(LED_TASK1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_TASK2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_TASK3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_CRITICAL, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_TASK1, 0);
    gpio_set_level(LED_TASK2, 0);
    gpio_set_level(LED_TASK3, 0);
    gpio_set_level(LED_CRITICAL, 0);

    // Initialize shared resource
    shared_data.counter = 0;
    strcpy(shared_data.shared_buffer, "Initial state");
    shared_data.checksum = calculate_checksum(shared_data.shared_buffer, shared_data.counter);
    shared_data.access_count = 0;

    // Create tasks (same priorities as before)
    xTaskCreate(high_priority_task, "HighPri", 3072, NULL, 5, NULL);
    xTaskCreate(medium_priority_task, "MedPri", 3072, NULL, 3, NULL);
    xTaskCreate(low_priority_task, "LowPri", 3072, NULL, 2, NULL);
    xTaskCreate(monitor_task, "Monitor", 3072, NULL, 1, NULL);

    ESP_LOGI(TAG, "System running WITHOUT MUTEX - expect data corruption!");
}
