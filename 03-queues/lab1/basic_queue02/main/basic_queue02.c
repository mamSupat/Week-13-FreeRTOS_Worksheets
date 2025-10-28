#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "QUEUE_LAB_EMPTY_TEST";

// LED pins
#define LED_SENDER GPIO_NUM_2
#define LED_RECEIVER GPIO_NUM_4

// Queue handle
QueueHandle_t xQueue;

// Data structure for queue messages
typedef struct {
    int id;
    char message[50];
    uint32_t timestamp;
} queue_message_t;

// Sender task (ส่งทุก 0.5 วินาทีเหมือนเดิม)
void sender_task(void *pvParameters) {
    queue_message_t message;
    int counter = 0;
    
    ESP_LOGI(TAG, "Sender task started (0.5s interval)");

    while (1) {
        message.id = counter++;
        snprintf(message.message, sizeof(message.message),
                 "Hello from sender #%d", message.id);
        message.timestamp = xTaskGetTickCount();

        BaseType_t xStatus = xQueueSend(xQueue, &message, pdMS_TO_TICKS(1000));

        if (xStatus == pdPASS) {
            ESP_LOGI(TAG, "✅ Sent: ID=%d, MSG=%s, Time=%lu | Queue count: %d", 
                     message.id, message.message, message.timestamp,
                     uxQueueMessagesWaiting(xQueue));

            gpio_set_level(LED_SENDER, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_SENDER, 0);
        } else {
            ESP_LOGW(TAG, "⚠️ Failed to send message (queue full?)");
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // ส่งทุก 0.5 วินาที
    }
}

// Receiver task (รับเร็วขึ้น → ทดสอบ Queue ว่าง)
void receiver_task(void *pvParameters) {
    queue_message_t received_message;

    ESP_LOGI(TAG, "Receiver task started (fast mode)");

    while (1) {
        BaseType_t xStatus = xQueueReceive(xQueue, &received_message, pdMS_TO_TICKS(2000));

        if (xStatus == pdPASS) {
            ESP_LOGI(TAG, "📩 Received: ID=%d, MSG=%s, Time=%lu", 
                     received_message.id, received_message.message, 
                     received_message.timestamp);

            gpio_set_level(LED_RECEIVER, 1);
            vTaskDelay(pdMS_TO_TICKS(50)); // Blink สั้นลง
            gpio_set_level(LED_RECEIVER, 0);

            // ประมวลผลเร็วขึ้น
            vTaskDelay(pdMS_TO_TICKS(100)); // แก้ตรงนี้ (จาก 1500 → 100 ms)
        } else {
            ESP_LOGW(TAG, "⏰ No message received within timeout (Queue empty?)");
        }
    }
}

// Queue monitoring task
void queue_monitor_task(void *pvParameters) {
    UBaseType_t uxMessagesWaiting;
    UBaseType_t uxSpacesAvailable;

    ESP_LOGI(TAG, "Queue monitor task started");

    while (1) {
        uxMessagesWaiting = uxQueueMessagesWaiting(xQueue);
        uxSpacesAvailable = uxQueueSpacesAvailable(xQueue);

        ESP_LOGI(TAG, "📊 Queue Status - Messages: %d, Free spaces: %d", 
                 uxMessagesWaiting, uxSpacesAvailable);

        printf("Queue: [");
        for (int i = 0; i < 5; i++) {
            if (i < uxMessagesWaiting) printf("■");
            else printf("□");
        }
        printf("]\n");

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "🧪 Queue Empty Test Starting...");

    gpio_set_direction(LED_SENDER, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RECEIVER, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_SENDER, 0);
    gpio_set_level(LED_RECEIVER, 0);

    xQueue = xQueueCreate(5, sizeof(queue_message_t));

    if (xQueue != NULL) {
        ESP_LOGI(TAG, "✅ Queue created successfully (size: 5 messages)");

        xTaskCreate(sender_task, "Sender", 2048, NULL, 2, NULL);
        xTaskCreate(receiver_task, "Receiver", 2048, NULL, 1, NULL);
        xTaskCreate(queue_monitor_task, "Monitor", 2048, NULL, 1, NULL);

        ESP_LOGI(TAG, "🚀 All tasks created. Starting scheduler...");
    } else {
        ESP_LOGE(TAG, "❌ Failed to create queue!");
    }
}
