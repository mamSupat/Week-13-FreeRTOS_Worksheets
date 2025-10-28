#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_random.h"

static const char *TAG = "PRIORITY_PRODUCTS_SHUTDOWN";

#define LED_PRODUCER_1 GPIO_NUM_2
#define LED_PRODUCER_2 GPIO_NUM_4
#define LED_PRODUCER_3 GPIO_NUM_5
#define LED_PRODUCER_4 GPIO_NUM_15
#define LED_CONSUMER_1 GPIO_NUM_18
#define LED_CONSUMER_2 GPIO_NUM_19

QueueHandle_t xProductQueue;
SemaphoreHandle_t xPrintMutex;

// ✅ เพิ่ม global shutdown flag
bool system_shutdown = false;

typedef struct {
    uint32_t produced;
    uint32_t consumed;
    uint32_t dropped;
} stats_t;

stats_t global_stats = {0, 0, 0};

typedef struct {
    int producer_id;
    int product_id;
    char product_name[30];
    uint32_t production_time;
    int processing_time_ms;
    int priority;
} product_t;

void safe_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    if (xSemaphoreTake(xPrintMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        vprintf(format, args);
        xSemaphoreGive(xPrintMutex);
    }
    va_end(args);
}

// ---------- Producer ----------
void producer_task(void *pvParameters) {
    int producer_id = *((int*)pvParameters);
    product_t product;
    int product_counter = 0;
    gpio_num_t led_pin;

    switch (producer_id) {
        case 1: led_pin = LED_PRODUCER_1; break;
        case 2: led_pin = LED_PRODUCER_2; break;
        case 3: led_pin = LED_PRODUCER_3; break;
        case 4: led_pin = LED_PRODUCER_4; break;
        default: led_pin = LED_PRODUCER_1;
    }

    safe_printf("Producer %d started\n", producer_id);

    while (!system_shutdown) {  // ✅ ใช้ flag ตรวจก่อนทำงานแต่ละรอบ
        product.producer_id = producer_id;
        product.product_id = product_counter++;
        snprintf(product.product_name, sizeof(product.product_name),
                 "Product-P%d-#%d", producer_id, product.product_id);
        product.production_time = xTaskGetTickCount();
        product.processing_time_ms = 500 + (esp_random() % 2000);
        product.priority = (esp_random() % 100 < 30) ? 1 : 0;

        if (xQueueSend(xProductQueue, &product, pdMS_TO_TICKS(100)) == pdPASS) {
            global_stats.produced++;
            safe_printf("✓ Producer %d: Created %s [Priority=%d]\n",
                        producer_id, product.product_name, product.priority);

            gpio_set_level(led_pin, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(led_pin, 0);
        } else {
            global_stats.dropped++;
            safe_printf("✗ Producer %d: Queue full! Dropped %s\n",
                        producer_id, product.product_name);
        }

        vTaskDelay(pdMS_TO_TICKS(1000 + (esp_random() % 1500)));
    }

    safe_printf("🛑 Producer %d stopped gracefully.\n", producer_id);
    vTaskDelete(NULL);
}

// ---------- Consumer ----------
void consumer_task(void *pvParameters) {
    int consumer_id = *((int*)pvParameters);
    product_t selected_product;
    gpio_num_t led_pin = (consumer_id == 1) ? LED_CONSUMER_1 : LED_CONSUMER_2;

    safe_printf("Consumer %d started\n", consumer_id);

    while (!system_shutdown) {  // ✅ ตรวจสถานะ shutdown
        if (xQueueReceive(xProductQueue, &selected_product, pdMS_TO_TICKS(2000)) == pdPASS) {
            global_stats.consumed++;
            uint32_t q_time = xTaskGetTickCount() - selected_product.production_time;

            safe_printf("→ Consumer %d: Processing %s [Priority=%d] (queue time: %lu ms)\n",
                        consumer_id, selected_product.product_name, selected_product.priority,
                        q_time * portTICK_PERIOD_MS);

            gpio_set_level(led_pin, 1);
            vTaskDelay(pdMS_TO_TICKS(selected_product.processing_time_ms));
            gpio_set_level(led_pin, 0);

            safe_printf("✓ Consumer %d: Finished %s\n",
                        consumer_id, selected_product.product_name);
        } else {
            safe_printf("⏰ Consumer %d: No products to process\n", consumer_id);
        }
    }

    safe_printf("🛑 Consumer %d stopped gracefully.\n", consumer_id);
    vTaskDelete(NULL);
}

// ---------- Statistics ----------
void statistics_task(void *pvParameters) {
    while (!system_shutdown) {
        UBaseType_t queue_items = uxQueueMessagesWaiting(xProductQueue);
        safe_printf("\n═══ SYSTEM STATISTICS ═══\n");
        safe_printf("Produced: %lu\n", global_stats.produced);
        safe_printf("Consumed: %lu\n", global_stats.consumed);
        safe_printf("Dropped : %lu\n", global_stats.dropped);
        safe_printf("Queue Backlog: %d\n", queue_items);
        safe_printf("Efficiency: %.1f %%\n",
                    global_stats.produced > 0 ?
                    (float)global_stats.consumed / global_stats.produced * 100 : 0);
        printf("Queue: [");
        for (int i = 0; i < 10; i++)
            printf(i < queue_items ? "■" : "□");
        printf("]\n═══════════════════════════\n\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    safe_printf("📊 Statistics task stopped.\n");
    vTaskDelete(NULL);
}

// ---------- Shutdown Task ----------
void shutdown_task(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(60000)); // ✅ Shutdown หลังจาก 60 วินาที (1 นาที)
    safe_printf("\n⚠️ Initiating system shutdown...\n");

    system_shutdown = true; // ตั้ง flag ให้ทุก task หยุด

    vTaskDelay(pdMS_TO_TICKS(3000)); // รอให้ task อื่น ๆ ปิดตัว
    safe_printf("✅ All tasks have been stopped gracefully.\n");
    vTaskDelete(NULL);
}

// ---------- Main ----------
void app_main(void) {
    ESP_LOGI(TAG, "Priority Products System (Graceful Shutdown) Starting...");

    gpio_set_direction(LED_PRODUCER_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PRODUCER_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PRODUCER_3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PRODUCER_4, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_CONSUMER_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_CONSUMER_2, GPIO_MODE_OUTPUT);

    xProductQueue = xQueueCreate(10, sizeof(product_t));
    xPrintMutex = xSemaphoreCreateMutex();

    if (xProductQueue && xPrintMutex) {
        static int p1 = 1, p2 = 2, p3 = 3, p4 = 4;
        static int c1 = 1, c2 = 2;

        xTaskCreate(producer_task, "Producer1", 3072, &p1, 3, NULL);
        xTaskCreate(producer_task, "Producer2", 3072, &p2, 3, NULL);
        xTaskCreate(producer_task, "Producer3", 3072, &p3, 3, NULL);
        xTaskCreate(producer_task, "Producer4", 3072, &p4, 3, NULL);

        xTaskCreate(consumer_task, "Consumer1", 3072, &c1, 2, NULL);
        xTaskCreate(consumer_task, "Consumer2", 3072, &c2, 2, NULL);

        xTaskCreate(statistics_task, "Statistics", 3072, NULL, 1, NULL);
        xTaskCreate(shutdown_task, "Shutdown", 2048, NULL, 1, NULL); // ✅ Task ใหม่สำหรับหยุดระบบ

        ESP_LOGI(TAG, "System running with graceful shutdown support.");
    } else {
        ESP_LOGE(TAG, "Failed to create queue or mutex!");
    }
}
