#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "QUEUE_SETS";

// --------- Queue handles ---------
QueueHandle_t xQueueUser, xQueueNetwork, xQueueTimer;  // ❌ Sensor queue ถูกปิดใช้งาน
QueueSetHandle_t xQueueSet;

// --------- Message structure ---------
typedef struct {
    char source[20];
    char content[50];
    int priority;
} message_t;

// --------- Safe print helper ---------
void print_message(const char *prefix, message_t *msg) {
    ESP_LOGI(TAG, "%s [%s]: %s (P:%d)",
             prefix, msg->source, msg->content, msg->priority);
}

// --------- USER Input Task ---------
void user_task(void *pv) {
    message_t msg;
    while (1) {
        snprintf(msg.source, sizeof(msg.source), "USER");
        snprintf(msg.content, sizeof(msg.content), "Button 1 pressed");
        msg.priority = 1;
        xQueueSend(xQueueUser, &msg, 0);
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

// --------- NETWORK Task ---------
void network_task(void *pv) {
    message_t msg;
    int cycle = 0;
    while (1) {
        snprintf(msg.source, sizeof(msg.source), "NETWORK");
        if (cycle % 3 == 0)
            snprintf(msg.content, sizeof(msg.content), "[WiFi] Heartbeat signal");
        else if (cycle % 3 == 1)
            snprintf(msg.content, sizeof(msg.content), "[Ethernet] Status update");
        else
            snprintf(msg.content, sizeof(msg.content), "[LoRa] Configuration changed");
        msg.priority = (cycle % 3 == 2) ? 5 : 2;
        xQueueSend(xQueueNetwork, &msg, 0);
        cycle++;
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// --------- TIMER Task ---------
void timer_task(void *pv) {
    message_t msg;
    int count = 0;
    while (1) {
        snprintf(msg.source, sizeof(msg.source), "TIMER");
        snprintf(msg.content, sizeof(msg.content), "Periodic maintenance");
        msg.priority = 1;
        xQueueSend(xQueueTimer, &msg, 0);
        count++;
        vTaskDelay(pdMS_TO_TICKS(7000));
    }
}

// --------- SYSTEM MONITOR Task ---------
void monitor_task(void *pv) {
    message_t msg;
    QueueSetMemberHandle_t xActivatedQueue;

    ESP_LOGI(TAG, "System monitor started");

    while (1) {
        xActivatedQueue = xQueueSelectFromSet(xQueueSet, pdMS_TO_TICKS(10000));
        if (xActivatedQueue != NULL) {
            if (xQueueReceive(xActivatedQueue, &msg, 0) == pdPASS) {
                ESP_LOGI(TAG, "→ Processing %s msg: %s", msg.source, msg.content);
                print_message("🌐", &msg);
            }
        } else {
            ESP_LOGW(TAG, "⚠️ No data received (Timeout)");
        }
    }
}

// --------- APP MAIN ---------
void app_main(void) {
    ESP_LOGI(TAG, "Experiment #2 - Disable Sensor Source Starting...");

    // สร้าง queues
    xQueueUser = xQueueCreate(5, sizeof(message_t));
    xQueueNetwork = xQueueCreate(5, sizeof(message_t));
    xQueueTimer = xQueueCreate(5, sizeof(message_t));

    // สร้าง queue set
    xQueueSet = xQueueCreateSet(15);

    // เพิ่ม queues เข้าชุด set
    xQueueAddToSet(xQueueUser, xQueueSet);
    xQueueAddToSet(xQueueNetwork, xQueueSet);
    xQueueAddToSet(xQueueTimer, xQueueSet);
    // ❌ Sensor ไม่ได้ถูกเพิ่ม

    // สร้าง tasks
    xTaskCreate(user_task, "User", 2048, NULL, 3, NULL);
    xTaskCreate(network_task, "Network", 2048, NULL, 3, NULL);
    xTaskCreate(timer_task, "Timer", 2048, NULL, 3, NULL);
    // ❌ ปิดใช้งาน Sensor
    // xTaskCreate(sensor_task, "Sensor", 2048, NULL, 3, NULL);

    xTaskCreate(monitor_task, "Monitor", 4096, NULL, 2, NULL);

    ESP_LOGI(TAG, "All tasks created. System operational (Sensor disabled).");
}
