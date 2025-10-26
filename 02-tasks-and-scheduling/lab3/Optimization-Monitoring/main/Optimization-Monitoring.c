#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>     // ✅ สำหรับ PRIu32, PRId32 macro
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

#define LED_RUNNING     GPIO_NUM_2
#define LED_READY       GPIO_NUM_4
#define LED_BLOCKED     GPIO_NUM_5
#define LED_SUSPENDED   GPIO_NUM_18
#define LED_WARNING     GPIO_NUM_19

#define BUTTON1_PIN     GPIO_NUM_0
#define BUTTON2_PIN     GPIO_NUM_35

static const char *TAG = "TASK_STATES";

TaskHandle_t state_demo_task_handle = NULL;
TaskHandle_t control_task_handle = NULL;
TaskHandle_t external_delete_handle = NULL;
SemaphoreHandle_t demo_semaphore = NULL;

// --------------------------------
// State names
// --------------------------------
const char* state_names[] = {
    "Running", "Ready", "Blocked", "Suspended", "Deleted", "Invalid"
};

const char* get_state_name(eTaskState state) {
    if (state <= eDeleted) return state_names[state];
    return state_names[5];
}

// --------------------------------
// Exercise 1 – State Transition Counter
// --------------------------------
volatile uint32_t state_changes[5] = {0};

void count_state_change(eTaskState old_state, eTaskState new_state) {
    if (old_state != new_state && new_state <= eDeleted) {
        state_changes[new_state]++;
        ESP_LOGI(TAG, "State change: %s → %s (Count: %" PRIu32 ")",
                 get_state_name(old_state),
                 get_state_name(new_state),
                 state_changes[new_state]);
    }
}

// --------------------------------
// Exercise 2 – Custom LED State Indicator
// --------------------------------
void update_state_display(eTaskState current_state) {
    gpio_set_level(LED_RUNNING, 0);
    gpio_set_level(LED_READY, 0);
    gpio_set_level(LED_BLOCKED, 0);
    gpio_set_level(LED_SUSPENDED, 0);

    switch (current_state) {
        case eRunning:   gpio_set_level(LED_RUNNING, 1); break;
        case eReady:     gpio_set_level(LED_READY, 1);   break;
        case eBlocked:   gpio_set_level(LED_BLOCKED, 1); break;
        case eSuspended: gpio_set_level(LED_SUSPENDED, 1); break;
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

// --------------------------------
// Step 1 – Basic Task States Demo
// --------------------------------
void state_demo_task(void *pvParameters) {
    ESP_LOGI(TAG, "State Demo Task started");
    int cycle = 0;
    eTaskState prev_state = eRunning;

    while (1) {
        cycle++;
        ESP_LOGI(TAG, "=== Cycle %d ===", cycle);
        eTaskState cur_state = eRunning;
        count_state_change(prev_state, cur_state);
        prev_state = cur_state;
        update_state_display(cur_state);

        ESP_LOGI(TAG, "Task is RUNNING");
        for (int i = 0; i < 1000000; i++) { volatile int dummy = i * 2; }

        cur_state = eReady;
        count_state_change(prev_state, cur_state);
        prev_state = cur_state;
        update_state_display(cur_state);
        taskYIELD();
        vTaskDelay(pdMS_TO_TICKS(100));

        cur_state = eBlocked;
        count_state_change(prev_state, cur_state);
        prev_state = cur_state;
        update_state_display(cur_state);
        ESP_LOGI(TAG, "Task will be BLOCKED (waiting for semaphore)");

        if (xSemaphoreTake(demo_semaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
            ESP_LOGI(TAG, "Got semaphore! RUNNING again");
            cur_state = eRunning;
            count_state_change(prev_state, cur_state);
            prev_state = cur_state;
            update_state_display(cur_state);
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            ESP_LOGW(TAG, "Semaphore timeout!");
        }

        cur_state = eBlocked;
        count_state_change(prev_state, cur_state);
        prev_state = cur_state;
        update_state_display(cur_state);
        ESP_LOGI(TAG, "Task is BLOCKED (in delay)");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void ready_state_demo_task(void *pvParameters) {
    while (1) {
        ESP_LOGI(TAG, "Ready state demo task running");
        for (int i = 0; i < 100000; i++) { volatile int dummy = i; }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// --------------------------------
// Control Task + System Monitor
// --------------------------------
void control_task(void *pvParameters) {
    ESP_LOGI(TAG, "Control Task started");
    bool suspended = false;
    int control_cycle = 0;
    static bool external_deleted = false;

    while (1) {
        control_cycle++;

        if (gpio_get_level(BUTTON1_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (!suspended) {
                ESP_LOGW(TAG, "=== SUSPEND Demo Task ===");
                vTaskSuspend(state_demo_task_handle);
                gpio_set_level(LED_SUSPENDED, 1);
                suspended = true;
            } else {
                ESP_LOGW(TAG, "=== RESUME Demo Task ===");
                vTaskResume(state_demo_task_handle);
                gpio_set_level(LED_SUSPENDED, 0);
                suspended = false;
            }
            while (gpio_get_level(BUTTON1_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (gpio_get_level(BUTTON2_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            ESP_LOGW(TAG, "=== GIVING SEMAPHORE ===");
            xSemaphoreGive(demo_semaphore);
            while (gpio_get_level(BUTTON2_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (control_cycle == 150 && !external_deleted) {
            ESP_LOGW(TAG, "Deleting external task");
            vTaskDelete(external_delete_handle);
            external_deleted = true;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// --------------------------------
// Step 2 – Stack Overflow Detection
// --------------------------------
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    ESP_LOGE("STACK_OVERFLOW", "Task %s overflowed its stack!", pcTaskName);
    ESP_LOGE("STACK_OVERFLOW", "Restarting...");
    for (int i = 0; i < 20; i++) {
        gpio_set_level(LED_WARNING, 1);
        vTaskDelay(pdMS_TO_TICKS(25));
        gpio_set_level(LED_WARNING, 0);
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    esp_restart();
}

// --------------------------------
// Step 3 – Stack Optimization
// --------------------------------
void optimized_heavy_task(void *pvParameters) {
    ESP_LOGI(TAG, "Optimized Heavy Task started");
    char *large_buffer = malloc(1024);
    int *large_numbers = malloc(200 * sizeof(int));
    char *another_buffer = malloc(512);
    if (!large_buffer || !large_numbers || !another_buffer) {
        ESP_LOGE(TAG, "Failed to allocate heap memory");
        free(large_buffer); free(large_numbers); free(another_buffer);
        vTaskDelete(NULL);
        return;
    }
    int cycle = 0;
    while (1) {
        cycle++;
        ESP_LOGI(TAG, "Optimized cycle %d: using heap", cycle);
        memset(large_buffer, 'Y', 1023); large_buffer[1023] = '\0';
        for (int i = 0; i < 200; i++) large_numbers[i] = i * cycle;
        snprintf(another_buffer, 512, "Optimized cycle %d", cycle);
        UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Stack remaining: %" PRIu32 " bytes",
                 (uint32_t)(stack_remaining * sizeof(StackType_t)));
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

// --------------------------------
// Exercise 1 – Stack Size Optimization
// --------------------------------
void heavy_stack_task(void *pvParameters) {
    ESP_LOGI(TAG, "Heavy stack task started");
    char big_array[1024];
    for (int i = 0; i < 1024; i++) big_array[i] = 'A';
    int cycle = 0;
    while (1) {
        cycle++;
        ESP_LOGI(TAG, "Heavy stack task cycle %d running...", cycle);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void test_stack_sizes(void) {
    uint32_t test_sizes[] = {512, 1024, 2048, 4096};
    for (int i = 0; i < 4; i++) {
        char task_name[20];
        snprintf(task_name, sizeof(task_name), "Test%" PRIu32, test_sizes[i]);
        BaseType_t result = xTaskCreate(heavy_stack_task, task_name, test_sizes[i], NULL, 1, NULL);
        ESP_LOGI(TAG, "Task with %" PRIu32 " bytes stack: %s",
                 test_sizes[i], result == pdPASS ? "Created" : "Failed");
    }
}

// --------------------------------
// Exercise 2 – Dynamic Stack Monitoring
// --------------------------------
void dynamic_stack_monitor(TaskHandle_t task_handle, const char* task_name) {
    static UBaseType_t previous_remaining = 0;
    UBaseType_t current_remaining = uxTaskGetStackHighWaterMark(task_handle);

    if (previous_remaining != 0 && current_remaining < previous_remaining) {
        ESP_LOGW(TAG, "%s stack usage increased by %" PRIu32 " bytes",
                 task_name,
                 (uint32_t)((previous_remaining - current_remaining) * sizeof(StackType_t)));
    }
    previous_remaining = current_remaining;
}

// --------------------------------
// App Main
// --------------------------------
void app_main(void) {
    ESP_LOGI(TAG, "=== FreeRTOS Task State + Stack Optimization Demo ===");

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL<<LED_RUNNING)|(1ULL<<LED_READY)|
                        (1ULL<<LED_BLOCKED)|(1ULL<<LED_SUSPENDED)|(1ULL<<LED_WARNING),
        .pull_down_en = 0, .pull_up_en = 0
    };
    gpio_config(&io_conf);

    gpio_config_t btn_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL<<BUTTON1_PIN)|(1ULL<<BUTTON2_PIN),
        .pull_up_en = 1, .pull_down_en = 0
    };
    gpio_config(&btn_conf);

    demo_semaphore = xSemaphoreCreateBinary();
    if (!demo_semaphore) { ESP_LOGE(TAG, "Create semaphore fail"); return; }

    static int self_delete_time = 10;
    xTaskCreate(state_demo_task, "StateDemo", 4096, NULL, 3, &state_demo_task_handle);
    xTaskCreate(ready_state_demo_task, "ReadyDemo", 2048, NULL, 3, NULL);
    xTaskCreate(control_task, "Control", 3072, NULL, 4, &control_task_handle);
    xTaskCreate(optimized_heavy_task, "OptimizedTask", 3072, NULL, 3, NULL);
    xTaskCreate(heavy_stack_task, "HeavyStack", 2048, NULL, 2, NULL);
    test_stack_sizes();

    ESP_LOGI(TAG, "All tasks created successfully.");
}
