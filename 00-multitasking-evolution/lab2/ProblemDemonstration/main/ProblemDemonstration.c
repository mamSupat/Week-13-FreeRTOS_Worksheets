#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

/* ===================== Pins ===================== */
#define LED1_PIN GPIO_NUM_2
#define LED2_PIN GPIO_NUM_4
#define LED3_PIN GPIO_NUM_5
#define LED4_PIN GPIO_NUM_18

/* ===================== Logging ===================== */
static const char *TAG = "TIME_SHARING";

/* ===================== Task IDs ===================== */
typedef enum {
    TASK_SENSOR = 0,
    TASK_PROCESS,
    TASK_ACTUATOR,
    TASK_DISPLAY,
    TASK_COUNT
} task_id_t;

/* ===================== Config ===================== */
#define TIME_SLICE_MS               50      // base time slice
#define RUN_EXPERIMENT_AT_START     1       // 1 = run experiment first
#define RUN_PROBLEM_DEMO_AT_START   1       // 1 = show problem demo at start

/* ===================== Globals (Stats) ===================== */
static uint32_t task_counter = 0;
static uint64_t context_switch_time = 0;
static uint32_t context_switches = 0;

/* ===================== Workloads ===================== */
static inline void simulate_sensor_task(void)
{
    static uint32_t sensor_count = 0;
    ESP_LOGI(TAG, "Sensor Task %u", sensor_count++);
    gpio_set_level(LED1_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));  // visible LED blink
    gpio_set_level(LED1_PIN, 0);
}

static inline void simulate_processing_task(void)
{
    static uint32_t process_count = 0;
    ESP_LOGI(TAG, "Processing Task %u", process_count++);
    gpio_set_level(LED2_PIN, 1);
    for (int i = 0; i < 100000; i++) { volatile int d = i * i; (void)d; }
    gpio_set_level(LED2_PIN, 0);
}

static inline void simulate_actuator_task(void)
{
    static uint32_t actuator_count = 0;
    ESP_LOGI(TAG, "Actuator Task %u", actuator_count++);
    gpio_set_level(LED3_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(LED3_PIN, 0);
}

static inline void simulate_display_task(void)
{
    static uint32_t display_count = 0;
    ESP_LOGI(TAG, "Display Task %u", display_count++);
    gpio_set_level(LED4_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(LED4_PIN, 0);
}

/* ===================== Manual Scheduler ===================== */
static inline void manual_scheduler(void)
{
    uint64_t start_time = esp_timer_get_time();

    context_switches++;
    for (int i = 0; i < 1000; i++) { volatile int d = i; (void)d; }

    switch (task_counter % TASK_COUNT) {
        case TASK_SENSOR:   simulate_sensor_task();    break;
        case TASK_PROCESS:  simulate_processing_task();break;
        case TASK_ACTUATOR: simulate_actuator_task();  break;
        case TASK_DISPLAY:  simulate_display_task();   break;
        default: break;
    }

    for (int i = 0; i < 1000; i++) { volatile int d = i; (void)d; }

    uint64_t end_time = esp_timer_get_time();
    context_switch_time += (end_time - start_time);
    task_counter++;
}

/* ===================== Stats Helper ===================== */
static inline void print_round_stats(uint32_t round_count, uint64_t start_time_us)
{
    uint64_t now = esp_timer_get_time();
    uint64_t total_time = now - start_time_us;
    float utilization = (float)context_switch_time * 100.0f / (float)total_time;
    float overhead_pct = 100.0f - utilization;

    ESP_LOGI(TAG, "=== Round %u Statistics ===", round_count);
    ESP_LOGI(TAG, "Context switches: %u", context_switches);
    ESP_LOGI(TAG, "Total time: %llu us", (unsigned long long)total_time);
    ESP_LOGI(TAG, "Task execution time: %llu us", (unsigned long long)context_switch_time);
    ESP_LOGI(TAG, "CPU utilization: %.1f%%", utilization);
    ESP_LOGI(TAG, "Overhead: %.1f%%", overhead_pct);
    ESP_LOGI(TAG, "Avg time per task: %llu us",
             (unsigned long long)(context_switches ? (context_switch_time / context_switches) : 0ULL));
}

/* ===================== Variable Time Slice Experiment ===================== */
static void variable_time_slice_experiment(void)
{
    ESP_LOGI(TAG, "\n=== Variable Time Slice Experiment (Fixed 5s per slice) ===");

    uint32_t time_slices[] = {10, 25, 50, 100, 200};
    const int num_slices = sizeof(time_slices) / sizeof(time_slices[0]);

    for (int i = 0; i < num_slices; i++) {
        uint32_t ts = time_slices[i];
        ESP_LOGI(TAG, "Testing time slice: %u ms", ts);

        context_switches = 0;
        context_switch_time = 0;
        task_counter = 0;

        uint64_t start = esp_timer_get_time();
        while ((esp_timer_get_time() - start) < 5ULL * 1000000ULL) { // 5 seconds
            manual_scheduler();
            vTaskDelay(pdMS_TO_TICKS(ts));
        }

        uint64_t duration = esp_timer_get_time() - start;
        float efficiency = (float)context_switch_time * 100.0f / (float)duration;

        ESP_LOGI(TAG, "Time slice %u ms: Efficiency %.1f%%", ts, efficiency);
        ESP_LOGI(TAG, "Context switches: %u", context_switches);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "=== Experiment finished ===\n");
}

/* ===================== Problem Demonstration ===================== */
void demonstrate_problems(void)
{
    ESP_LOGI(TAG, "\n=== Demonstrating Time-Sharing Problems ===");

    // Problem 1: No priority support
    ESP_LOGI(TAG, "Problem 1: No priority support");
    ESP_LOGI(TAG, "Critical task must wait for less important tasks");

    // Problem 2: Fixed time slice issues
    ESP_LOGI(TAG, "Problem 2: Fixed time slice problems");
    ESP_LOGI(TAG, "Short tasks waste time, long tasks get interrupted");

    // Problem 3: Context switching overhead
    ESP_LOGI(TAG, "Problem 3: Context switching overhead");
    ESP_LOGI(TAG, "Time wasted in switching between tasks");

    // Problem 4: No inter-task communication
    ESP_LOGI(TAG, "Problem 4: No proper inter-task communication");
    ESP_LOGI(TAG, "Tasks cannot communicate safely");
}

/* ===================== app_main ===================== */
void app_main(void)
{
    // GPIO setup
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED1_PIN) | (1ULL << LED2_PIN) |
                        (1ULL << LED3_PIN) | (1ULL << LED4_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Time-Sharing System Started");
    ESP_LOGI(TAG, "Base time slice: %d ms", TIME_SLICE_MS);

#if RUN_PROBLEM_DEMO_AT_START
    demonstrate_problems();
#endif

#if RUN_EXPERIMENT_AT_START
    variable_time_slice_experiment();
#endif

    uint64_t start_time = esp_timer_get_time();
    uint32_t round_count = 0;

    while (1) {
        manual_scheduler();
        vTaskDelay(pdMS_TO_TICKS(TIME_SLICE_MS));

        if (context_switches % 20 == 0) {
            round_count++;
            print_round_stats(round_count, start_time);
        }
    }
}
