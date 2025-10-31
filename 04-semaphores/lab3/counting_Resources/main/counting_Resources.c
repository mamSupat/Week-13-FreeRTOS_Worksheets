#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_random.h"

static const char *TAG = "COUNTING_SEM_EXP2";

// LED pins for visualization
#define LED_RESOURCE_1 GPIO_NUM_2
#define LED_RESOURCE_2 GPIO_NUM_4
#define LED_RESOURCE_3 GPIO_NUM_5
#define LED_RESOURCE_4 GPIO_NUM_16
#define LED_RESOURCE_5 GPIO_NUM_17
#define LED_PRODUCER GPIO_NUM_18
#define LED_SYSTEM GPIO_NUM_19

// Configuration
#define MAX_RESOURCES 5   // ✅ เพิ่มจำนวน Resource เป็น 5
#define NUM_PRODUCERS 5
#define NUM_CONSUMERS 3

// Semaphore handle
SemaphoreHandle_t xCountingSemaphore;

// Resource management
typedef struct {
    int resource_id;
    bool in_use;
    char current_user[20];
    uint32_t usage_count;
    uint32_t total_usage_time;
} resource_t;

resource_t resources[MAX_RESOURCES] = {
    {1, false, "", 0, 0},
    {2, false, "", 0, 0},
    {3, false, "", 0, 0},
    {4, false, "", 0, 0},
    {5, false, "", 0, 0}
};

// System statistics
typedef struct {
    uint32_t total_requests;
    uint32_t successful_acquisitions;
    uint32_t failed_acquisitions;
    uint32_t resources_in_use;
} system_stats_t;

system_stats_t stats = {0, 0, 0, 0};

// ===== Resource handling =====
int acquire_resource(const char* user_name) {
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (!resources[i].in_use) {
            resources[i].in_use = true;
            strcpy(resources[i].current_user, user_name);
            resources[i].usage_count++;
            
            // LED ON
            switch (i) {
                case 0: gpio_set_level(LED_RESOURCE_1, 1); break;
                case 1: gpio_set_level(LED_RESOURCE_2, 1); break;
                case 2: gpio_set_level(LED_RESOURCE_3, 1); break;
                case 3: gpio_set_level(LED_RESOURCE_4, 1); break;
                case 4: gpio_set_level(LED_RESOURCE_5, 1); break;
            }
            
            stats.resources_in_use++;
            return i;
        }
    }
    return -1;
}

void release_resource(int resource_index, uint32_t usage_time) {
    if (resource_index >= 0 && resource_index < MAX_RESOURCES) {
        resources[resource_index].in_use = false;
        strcpy(resources[resource_index].current_user, "");
        resources[resource_index].total_usage_time += usage_time;
        
        // LED OFF
        switch (resource_index) {
            case 0: gpio_set_level(LED_RESOURCE_1, 0); break;
            case 1: gpio_set_level(LED_RESOURCE_2, 0); break;
            case 2: gpio_set_level(LED_RESOURCE_3, 0); break;
            case 3: gpio_set_level(LED_RESOURCE_4, 0); break;
            case 4: gpio_set_level(LED_RESOURCE_5, 0); break;
        }
        
        stats.resources_in_use--;
    }
}

// ===== Producer tasks =====
void producer_task(void *pvParameters) {
    int producer_id = *((int*)pvParameters);
    char task_name[20];
    snprintf(task_name, sizeof(task_name), "Producer%d", producer_id);
    
    ESP_LOGI(TAG, "%s started", task_name);
    
    while (1) {
        stats.total_requests++;
        ESP_LOGI(TAG, "🏭 %s: Requesting resource...", task_name);
        
        gpio_set_level(LED_PRODUCER, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(LED_PRODUCER, 0);
        
        uint32_t start_time = xTaskGetTickCount();
        
        if (xSemaphoreTake(xCountingSemaphore, pdMS_TO_TICKS(8000)) == pdTRUE) {
            stats.successful_acquisitions++;
            uint32_t wait_time = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
            
            int res_idx = acquire_resource(task_name);
            if (res_idx >= 0) {
                ESP_LOGI(TAG, "✓ %s: Acquired resource %d (wait %lums)", 
                         task_name, res_idx + 1, wait_time);
                
                uint32_t usage_time = 1000 + (esp_random() % 3000);
                ESP_LOGI(TAG, "🔧 %s: Using resource %d for %lums", 
                         task_name, res_idx + 1, usage_time);
                vTaskDelay(pdMS_TO_TICKS(usage_time));
                
                release_resource(res_idx, usage_time);
                ESP_LOGI(TAG, "✓ %s: Released resource %d", task_name, res_idx + 1);
                
                xSemaphoreGive(xCountingSemaphore);
            } else {
                ESP_LOGE(TAG, "✗ %s: No resource available!", task_name);
                xSemaphoreGive(xCountingSemaphore);
            }
        } else {
            stats.failed_acquisitions++;
            ESP_LOGW(TAG, "⏰ %s: Timeout waiting for resource", task_name);
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000 + (esp_random() % 3000)));
    }
}

// ===== Resource Monitor =====
void resource_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Resource monitor started");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        int available = uxSemaphoreGetCount(xCountingSemaphore);
        ESP_LOGI(TAG, "\n📊 RESOURCE STATUS (%d/%d available)", available, MAX_RESOURCES);
        
        for (int i = 0; i < MAX_RESOURCES; i++) {
            if (resources[i].in_use) {
                ESP_LOGI(TAG, "Resource %d: BUSY (User: %s, Used: %lu times)", 
                         i + 1, resources[i].current_user, resources[i].usage_count);
            } else {
                ESP_LOGI(TAG, "Resource %d: FREE (Total used: %lu times)", 
                         i + 1, resources[i].usage_count);
            }
        }
        
        printf("Pool: [");
        for (int i = 0; i < MAX_RESOURCES; i++) {
            printf(resources[i].in_use ? "■" : "□");
        }
        printf("] Available: %d\n", available);
        
        ESP_LOGI(TAG, "══════════════════════════════\n");
    }
}

// ===== System Statistics =====
void statistics_task(void *pvParameters) {
    ESP_LOGI(TAG, "Statistics task started");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(12000));
        
        ESP_LOGI(TAG, "\n📈 SYSTEM STATISTICS");
        ESP_LOGI(TAG, "Total requests: %lu", stats.total_requests);
        ESP_LOGI(TAG, "Successful: %lu", stats.successful_acquisitions);
        ESP_LOGI(TAG, "Failed: %lu", stats.failed_acquisitions);
        ESP_LOGI(TAG, "Resources in use: %lu", stats.resources_in_use);
        
        float success_rate = stats.total_requests > 0 
            ? (float)stats.successful_acquisitions / stats.total_requests * 100 : 0;
        ESP_LOGI(TAG, "Success rate: %.1f%%", success_rate);
        
        for (int i = 0; i < MAX_RESOURCES; i++) {
            ESP_LOGI(TAG, "Resource %d: %lu uses, %lu total ms", 
                     i + 1, resources[i].usage_count, resources[i].total_usage_time);
        }
        ESP_LOGI(TAG, "══════════════════════════════\n");
    }
}

// ===== Load Generator =====
void load_generator_task(void *pvParameters) {
    ESP_LOGI(TAG, "Load generator started");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20000));
        ESP_LOGW(TAG, "🚀 LOAD BURST START");
        gpio_set_level(LED_SYSTEM, 1);
        
        for (int burst = 0; burst < 3; burst++) {
            ESP_LOGI(TAG, "Load burst %d/3", burst + 1);
            for (int i = 0; i < MAX_RESOURCES + 2; i++) {
                if (xSemaphoreTake(xCountingSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
                    int idx = acquire_resource("LoadGen");
                    if (idx >= 0) {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        release_resource(idx, 500);
                    }
                    xSemaphoreGive(xCountingSemaphore);
                } else {
                    ESP_LOGW(TAG, "LoadGen: Resource pool full");
                }
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        gpio_set_level(LED_SYSTEM, 0);
        ESP_LOGI(TAG, "LOAD BURST COMPLETED\n");
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Counting Semaphores Lab - Experiment 2 (5 Resources)");
    
    // Configure LEDs
    gpio_set_direction(LED_RESOURCE_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RESOURCE_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RESOURCE_3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RESOURCE_4, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RESOURCE_5, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PRODUCER, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_SYSTEM, GPIO_MODE_OUTPUT);
    
    // Turn off all LEDs
    gpio_set_level(LED_RESOURCE_1, 0);
    gpio_set_level(LED_RESOURCE_2, 0);
    gpio_set_level(LED_RESOURCE_3, 0);
    gpio_set_level(LED_RESOURCE_4, 0);
    gpio_set_level(LED_RESOURCE_5, 0);
    gpio_set_level(LED_PRODUCER, 0);
    gpio_set_level(LED_SYSTEM, 0);
    
    // Create Counting Semaphore (max = 5)
    xCountingSemaphore = xSemaphoreCreateCounting(MAX_RESOURCES, MAX_RESOURCES);
    
    if (xCountingSemaphore != NULL) {
        ESP_LOGI(TAG, "Counting semaphore created (max: %d)", MAX_RESOURCES);
        
        static int producer_ids[NUM_PRODUCERS] = {1, 2, 3, 4, 5};
        for (int i = 0; i < NUM_PRODUCERS; i++) {
            char name[20];
            snprintf(name, sizeof(name), "Producer%d", i + 1);
            xTaskCreate(producer_task, name, 3072, &producer_ids[i], 3, NULL);
        }
        
        xTaskCreate(resource_monitor_task, "ResMonitor", 3072, NULL, 2, NULL);
        xTaskCreate(statistics_task, "Statistics", 3072, NULL, 1, NULL);
        xTaskCreate(load_generator_task, "LoadGen", 2048, NULL, 4, NULL);
        
        ESP_LOGI(TAG, "System created with 5 resources and %d producers", NUM_PRODUCERS);
        ESP_LOGI(TAG, "System operational - monitoring expanded resource pool!");
        
    } else {
        ESP_LOGE(TAG, "Failed to create counting semaphore!");
    }
}
