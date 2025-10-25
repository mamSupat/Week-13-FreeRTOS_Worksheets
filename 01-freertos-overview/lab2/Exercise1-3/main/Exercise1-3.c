#include <stdio.h>
#include <stdarg.h>         // สำหรับ va_list
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

static const char *TAG = "EXERCISES";

/* -------------------------------------------------
 * Exercise 1: Custom Logger
 * ------------------------------------------------- */
#define LOG_COLOR_CYAN   "36"
#define LOG_BOLD(COLOR)  "\033[1;" COLOR "m"
#define LOG_RESET_COLOR  "\033[0m"

void custom_log(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    printf(LOG_BOLD(LOG_COLOR_CYAN) "[CUSTOM] %s: " LOG_RESET_COLOR, tag);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

/* -------------------------------------------------
 * Exercise 2: Performance Monitoring
 * ------------------------------------------------- */
void performance_demo(void)
{
    ESP_LOGI(TAG, "=== Performance Monitoring ===");

    uint64_t start_time = esp_timer_get_time();

    // งานจำลอง
    for (int i = 0; i < 1000000; i++) {
        volatile int dummy = i * 2;
        (void)dummy;
    }

    uint64_t end_time = esp_timer_get_time();
    uint64_t execution_time = end_time - start_time;

    ESP_LOGI(TAG, "Execution time: %llu microseconds", (unsigned long long)execution_time);
    ESP_LOGI(TAG, "Execution time: %.2f milliseconds", execution_time / 1000.0);
}

/* -------------------------------------------------
 * Exercise 3: Error Handling Demo
 * ------------------------------------------------- */
void error_handling_demo(void)
{
    ESP_LOGI(TAG, "=== Error Handling Demo ===");

    esp_err_t result;

    // Success
    result = ESP_OK;
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Operation completed successfully");
    }

    // Simulate error
    result = ESP_ERR_NO_MEM;
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Error: %s", esp_err_to_name(result));
    }

    // Non-fatal warning
    result = ESP_ERR_INVALID_ARG;
    ESP_ERROR_CHECK_WITHOUT_ABORT(result);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Non-fatal error: %s", esp_err_to_name(result));
    }
}

/* -------------------------------------------------
 * Main function
 * ------------------------------------------------- */
void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32 Logging Exercises ===");

    // เรียกแต่ละ exercise
    custom_log("SENSOR", "Temperature: %d°C", 25);
    performance_demo();
    error_handling_demo();

    ESP_LOGI(TAG, "All exercises executed successfully!");
}
