#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_RUNNING    GPIO_NUM_2
#define LED_READY      GPIO_NUM_4
#define LED_BLOCKED    GPIO_NUM_5
#define LED_SUSPENDED  GPIO_NUM_18

#define BUTTON1_PIN    GPIO_NUM_0
#define BUTTON2_PIN    GPIO_NUM_35

static const char *TAG = "TASK_STATES";

TaskHandle_t state_demo_task_handle = NULL;
TaskHandle_t control_task_handle = NULL;
TaskHandle_t external_delete_handle = NULL;

SemaphoreHandle_t demo_semaphore = NULL;

const char* state_names[] = {
    "Running", "Ready", "Blocked", "Suspended", "Deleted", "Invalid"
};

// ---------------- Helper ----------------
const char* get_state_name(eTaskState state) {
    if (state <= eDeleted) return state_names[state];
    return state_names[5];
}

// ---------------- Exercise 1: State Transition Counter ----------------
volatile uint32_t state_changes[5] = {0};
void count_state_change(eTaskState old_state, eTaskState new_state) {
    if (old_state != new_state && new_state <= eDeleted) {
        state_changes[new_state]++;
        ESP_LOGI(TAG, "State change: %s → %s (Count: %d)",
                 get_state_name(old_state),
                 get_state_name(new_state),
                 state_changes[new_state]);
    }
}

// ---------------- Exercise 2: Custom State Indicator ----------------
void update_state_display(eTaskState current_state) {
    gpio_set_level(LED_RUNNING, 0);
    gpio_set_level(LED_READY, 0);
    gpio_set_level(LED_BLOCKED, 0);
    gpio_set_level(LED_SUSPENDED, 0);

    switch (current_state) {
        case eRunning:
            gpio_set_level(LED_RUNNING, 1);
            break;
        case eReady:
            gpio_set_level(LED_READY, 1);
            break;
        case eBlocked:
            gpio_set_level(LED_BLOCKED, 1);
            break;
        case eSuspended:
            gpio_set_level(LED_SUSPENDED, 1);
            break;
        default:
            for (int i = 0; i < 3; i++) {
                gpio_set_level(LED_RUNNING, 1);
                gpio_set_level(LED_READY, 1);
                gpio_set_level(LED_BLOCKED, 1);
                gpio_set_level(LED_SUSPENDED, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_RUNNING, 0);
                gpio_set_level(LED_READY, 0);
                gpio_set_level(LED_BLOCKED, 0);
                gpio_set_level(LED_SUSPENDED, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;
    }
}

// ---------------- State Demo Task ----------------
void state_demo_task(void *pvParameters) {
    ESP_LOGI(TAG, "State Demo Task started");
    int cycle = 0;
    eTaskState old_state = eRunning;

    while (1) {
        cycle++;
        ESP_LOGI(TAG, "=== Cycle %d ===", cycle);

        // RUNNING
        update_state_display(eRunning);
        count_state_change(old_state, eRunning);
        old_state = eRunning;
        for (int i = 0; i < 1000000; i++) { volatile int dummy = i * 2; }

        // READY
        update_state_display(eReady);
        count_state_change(old_state, eReady);
        old_state = eReady;
        vTaskDelay(pdMS_TO_TICKS(100));

        // BLOCKED (รอ Semaphore)
        update_state_display(eBlocked);
        count_state_change(old_state, eBlocked);
        old_state = eBlocked;
        if (xSemaphoreTake(demo_semaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
            ESP_LOGI(TAG, "Got semaphore! RUNNING again");
        } else {
            ESP_LOGW(TAG, "Semaphore timeout!");
        }

        // BLOCKED (Delay)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------------- Ready Demo Task ----------------
void ready_state_demo_task(void *pvParameters) {
    while (1) {
        ESP_LOGI(TAG, "Ready state demo task running");
        for (int i = 0; i < 100000; i++) { volatile int dummy = i; }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// ---------------- Self-Deleting Task ----------------
void self_deleting_task(void *pvParameters) {
    int *lifetime = (int *)pvParameters;
    ESP_LOGI(TAG, "Self-deleting task will live for %d seconds", *lifetime);
    for (int i = *lifetime; i > 0; i--) {
        ESP_LOGI(TAG, "Countdown: %d", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Self-deleting task going to DELETED state");
    vTaskDelete(NULL);
}

// ---------------- External Delete Task ----------------
void external_delete_task(void *pvParameters) {
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "External delete task running: %d", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------------- Step 3: Detailed Monitor ----------------
void monitor_task_states(void) {
    ESP_LOGI(TAG, "=== DETAILED TASK STATE MONITOR ===");
    TaskHandle_t tasks[] = { state_demo_task_handle, control_task_handle, external_delete_handle };
    const char* names[]  = { "StateDemo", "Control", "ExtDelete" };
    for (int i = 0; i < 3; i++) {
        if (tasks[i] != NULL) {
            eTaskState s = eTaskGetState(tasks[i]);
            UBaseType_t pr = uxTaskPriorityGet(tasks[i]);
            UBaseType_t st = uxTaskGetStackHighWaterMark(tasks[i]);
            ESP_LOGI(TAG, "%s: State=%s, Priority=%d, Stack=%d bytes",
                     names[i], get_state_name(s), pr, st * sizeof(StackType_t));
        }
    }
}

// ---------------- Control Task ----------------
void control_task(void *pvParameters) {
    bool suspended = false;
    bool ext_deleted = false;
    int cycle = 0;
    ESP_LOGI(TAG, "Control Task started");

    while (1) {
        cycle++;

        // Button 1: Suspend/Resume
        if (gpio_get_level(BUTTON1_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (!suspended) {
                ESP_LOGW(TAG, "=== SUSPENDING Demo Task ===");
                vTaskSuspend(state_demo_task_handle);
                update_state_display(eSuspended);
                suspended = true;
            } else {
                ESP_LOGW(TAG, "=== RESUMING Demo Task ===");
                vTaskResume(state_demo_task_handle);
                suspended = false;
            }
            while (gpio_get_level(BUTTON1_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Button 2: Give Semaphore
        if (gpio_get_level(BUTTON2_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            ESP_LOGW(TAG, "=== GIVING SEMAPHORE ===");
            xSemaphoreGive(demo_semaphore);
            while (gpio_get_level(BUTTON2_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        // ลบ External Task หลัง 15 วินาที
        if (cycle == 150 && !ext_deleted) {
            ESP_LOGW(TAG, "Deleting external task");
            vTaskDelete(external_delete_handle);
            ext_deleted = true;
        }

        // แสดง Monitor ทุก 3 วินาที
        if (cycle % 30 == 0) monitor_task_states();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ---------------- System Monitor ----------------
void system_monitor_task(void *pvParameters) {
    char *list_buf = malloc(1024);
    char *stat_buf = malloc(1024);
    if (!list_buf || !stat_buf) { ESP_LOGE(TAG, "Buffer alloc fail"); vTaskDelete(NULL); }
    while (1) {
        ESP_LOGI(TAG, "\n=== SYSTEM MONITOR ===");
        vTaskList(list_buf);
        ESP_LOGI(TAG, "Name\tState\tPrio\tStack\tNum");
        ESP_LOGI(TAG, "%s", list_buf);
        vTaskGetRunTimeStats(stat_buf);
        ESP_LOGI(TAG, "\nRuntime Stats:\n%s", stat_buf);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ---------------- app_main ----------------
void app_main(void) {
    ESP_LOGI(TAG, "=== FreeRTOS Task State Full Demo ===");

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL<<LED_RUNNING)|(1ULL<<LED_READY)|(1ULL<<LED_BLOCKED)|(1ULL<<LED_SUSPENDED)
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
