#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "QUEUE_LAB_NONBLOCK";

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

// Sender task
void sender_task(void *pvParameters) {
    queue_message_t message;
    int counter = 0;

    ESP_LOGI(TAG, "Sender task started (0.5s interval)");

    while (1) {
        // Prepare message
        message.id = counter++;
        snprintf(message.message, sizeof(message.message),
                 "Hello from sender #%d", message.id);
        message.timestamp = xTaskGetTickCount();

        // Send with short wait
        if (xQueueSend(xQueue, &message, 0) != pdPASS) {
            ESP_LOGW(TAG, "🚫 Queue full! Dropping message ID=%d", message.id);
        } else {
            ESP_LOGI(TAG, "✅ Sent: ID=%d, MSG=%s, Time=%lu | Queue count: %d",
                     message.id, message.message, message.timestamp,
                     uxQueueMessagesWaiting(xQueue));

            gpio_set_level(LED_SENDER, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_SENDER, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Send every 0.5s
    }
}

// Receiver task (Non-blocking mode)
void receiver_task(void *pvParameters) {
    queue_message_t received_message;

    ESP_LOGI(TAG, "Receiver task started (non-blocking mode)");

    while (1) {
        // Non-blocking receive
        if (xQueueReceive(xQueue, &received_message, 0) == pdPASS) {
            // Process immediately
            ESP_LOGI(TAG, "📩 Received: ID=%d, MSG=%s, Time=%lu",
                     received_message.id, received_message.message,
                     received_message.timestamp);

            gpio_set_level(LED_RECEIVER, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(LED_RECEIVER, 0);
        } else {
            // No data, do other work
            ESP_LOGI(TAG, "💤 No message available, doing other work...");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// Queue monitor task
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

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "🧪 Non-blocking Queue Receive Test Starting...");

    // Configure GPIO
    gpio_set_direction(LED_SENDER, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RECEIVER, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_SENDER, 0);
    gpio_set_level(LED_RECEIVER, 0);

    // Create queue (size: 5 messages)
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
