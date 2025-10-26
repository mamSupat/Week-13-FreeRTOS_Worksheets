#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_RUNNING GPIO_NUM_2
#define LED_READY GPIO_NUM_4
#define LED_BLOCKED GPIO_NUM_5
#define LED_SUSPENDED GPIO_NUM_18

#define BUTTON1_PIN GPIO_NUM_0S
#define BUTTON2_PIN GPIO_NUM_35

static const char *TAG = "TASK_STATES";

// Task handles
TaskHandle_t state_demo_task_handle = NULL;
TaskHandle_t control_task_handle = NULL;
TaskHandle_t external_delete_handle = NULL;

// Semaphore
SemaphoreHandle_t demo_semaphore = NULL;

// State names
const char* state_names[] = {
    "Running", "Ready", "Blocked", "Suspended", "Deleted", "Invalid"
};

// ---------- Helper Function ----------
const char* get_state_name(eTaskState state)
{
    if (state <= eDeleted) return state_names[state];
    return state_names[5];
}

// ---------- State Demo Task ----------
void state_demo_task(void *pvParameters)
{
    ESP_LOGI(TAG, "State Demo Task started");
    int cycle = 0;

    while (1) {
        cycle++;
        ESP_LOGI(TAG, "=== Cycle %d ===", cycle);

        // Running
        ESP_LOGI(TAG, "Task is RUNNING");
        gpio_set_level(LED_RUNNING, 1);
        gpio_set_level(LED_READY, 0);
        gpio_set_level(LED_BLOCKED, 0);
        gpio_set_level(LED_SUSPENDED, 0);
        for (int i = 0; i < 1000000; i++) { volatile int dummy = i * 2; }

        // Ready
        ESP_LOGI(TAG, "Task will be READY (yielding)");
        gpio_set_level(LED_RUNNING, 0);
        gpio_set_level(LED_READY, 1);
        taskYIELD();
        vTaskDelay(pdMS_TO_TICKS(100));

        // Blocked - waiting for semaphore
        ESP_LOGI(TAG, "Task will be BLOCKED (waiting for semaphore)");
        gpio_set_level(LED_READY, 0);
        gpio_set_level(LED_BLOCKED, 1);

        if (xSemaphoreTake(demo_semaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
            ESP_LOGI(TAG, "Got semaphore! RUNNING again");
            gpio_set_level(LED_BLOCKED, 0);
            gpio_set_level(LED_RUNNING, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            ESP_LOGW(TAG, "Semaphore timeout!");
            gpio_set_level(LED_BLOCKED, 0);
        }

        // Delay (still Blocked)
        ESP_LOGI(TAG, "Task is BLOCKED (in vTaskDelay)");
        gpio_set_level(LED_BLOCKED, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(LED_BLOCKED, 0);
    }
}

// ---------- Ready State Demo ----------
void ready_state_demo_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Ready state demo task running");
        for (int i = 0; i < 100000; i++) { volatile int dummy = i; }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// ---------- Self-Deleting Task ----------
void self_deleting_task(void *pvParameters)
{
    int *lifetime = (int *)pvParameters;
    ESP_LOGI(TAG, "Self-deleting task will live for %d seconds", *lifetime);

    for (int i = *lifetime; i > 0; i--) {
        ESP_LOGI(TAG, "Countdown: %d", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Self-deleting task going to DELETED state");
    vTaskDelete(NULL);
}

// ---------- External Delete Task ----------
void external_delete_task(void *pvParameters)
{
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "External delete task running: %d", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------- DETAILED TASK MONITOR (STEP 3) ----------
void monitor_task_states(void)
{
    ESP_LOGI(TAG, "=== DETAILED TASK STATE MONITOR ===");
    TaskHandle_t tasks[] = {
        state_demo_task_handle,
        control_task_handle,
        external_delete_handle
    };
    const char* task_names[] = { "StateDemo", "Control", "ExtDelete" };
    int num_tasks = sizeof(tasks) / sizeof(tasks[0]);

    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i] != NULL) {
            eTaskState state = eTaskGetState(tasks[i]);
            UBaseType_t prio = uxTaskPriorityGet(tasks[i]);
            UBaseType_t stack_rem = uxTaskGetStackHighWaterMark(tasks[i]);
            ESP_LOGI(TAG, "%s: State=%s, Priority=%d, Stack=%d bytes",
                     task_names[i], get_state_name(state), prio,
                     stack_rem * sizeof(StackType_t));
        }
    }
}

// ---------- Control Task ----------
void control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Control Task started");
    bool suspended = false;
    bool external_deleted = false;
    int control_cycle = 0;

    while (1) {
        control_cycle++;

        // Button 1 = Suspend/Resume
        if (gpio_get_level(BUTTON1_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (!suspended) {
                ESP_LOGW(TAG, "=== SUSPENDING Demo Task ===");
                vTaskSuspend(state_demo_task_handle);
                gpio_set_level(LED_SUSPENDED, 1);
                suspended = true;
            } else {
                ESP_LOGW(TAG, "=== RESUMING Demo Task ===");
                vTaskResume(state_demo_task_handle);
                gpio_set_level(LED_SUSPENDED, 0);
                suspended = false;
            }
            while (gpio_get_level(BUTTON1_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Button 2 = Give semaphore
        if (gpio_get_level(BUTTON2_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            ESP_LOGW(TAG, "=== GIVING SEMAPHORE ===");
            xSemaphoreGive(demo_semaphore);
            while (gpio_get_level(BUTTON2_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        // After 15s delete external task
        if (control_cycle == 150 && !external_deleted) {
            ESP_LOGW(TAG, "Deleting external task");
            vTaskDelete(external_delete_handle);
            external_deleted = true;
        }

        // Show state every 3s
        if (control_cycle % 30 == 0) {
            monitor_task_states();  // 👈 เรียกฟังก์ชัน Step 3
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ---------- System Monitor ----------
void system_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "System Monitor started");
    char *task_buf = malloc(1024);
    char *stats_buf = malloc(1024);
    if (!task_buf || !stats_buf) { ESP_LOGE(TAG, "Malloc failed"); vTaskDelete(NULL); }

    while (1) {
        ESP_LOGI(TAG, "\n=== SYSTEM MONITOR ===");
        vTaskList(task_buf);
        ESP_LOGI(TAG, "Name\tState\tPrio\tStack\tNum");
        ESP_LOGI(TAG, "%s", task_buf);
        vTaskGetRunTimeStats(stats_buf);
        ESP_LOGI(TAG, "\nRuntime Stats:");
        ESP_LOGI(TAG, "%s", stats_buf);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ---------- app_main ----------
void app_main(void)
{
    ESP_LOGI(TAG, "=== FreeRTOS Task States + Advanced + Monitor Demo ===");

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL<<LED_RUNNING)|(1ULL<<LED_READY)|(1ULL<<LED_BLOCKED)|(1ULL<<LED_SUSPENDED),
    };
    gpio_config(&io_conf);

    gpio_config_t btn_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL<<BUTTON1_PIN)|(1ULL<<BUTTON2_PIN),
        .pull_up_en = 1
    };
    gpio_config(&btn_conf);

    demo_semaphore = xSemaphoreCreateBinary();
    if (!demo_semaphore) { ESP_LOGE(TAG, "Semaphore create fail"); return; }

    static int self_delete_time = 10;
    xTaskCreate(state_demo_task, "StateDemo", 4096, NULL, 3, &state_demo_task_handle);
    xTaskCreate(ready_state_demo_task, "ReadyDemo", 2048, NULL, 3, NULL);
    xTaskCreate(control_task, "Control", 3072, NULL, 4, &control_task_handle);
    xTaskCreate(system_monitor_task, "Monitor", 4096, NULL, 1, NULL);
    xTaskCreate(self_deleting_task, "SelfDelete", 2048, &self_delete_time, 2, NULL);
    xTaskCreate(external_delete_task, "ExtDelete", 2048, NULL, 2, &external_delete_handle);

    ESP_LOGI(TAG, "All tasks created. Monitoring task states...");
}
