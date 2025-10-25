#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "esp_chip_info.h"   // v5.x ต้อง include เอง
#include "esp_flash.h"       // ใช้ esp_flash_get_size()

static const char *TAG = "LOGGING_DEMO";

void demonstrate_logging_levels(void)
{
    ESP_LOGE(TAG, "This is an ERROR message - highest priority");
    ESP_LOGW(TAG, "This is a WARNING message");
    ESP_LOGI(TAG, "This is an INFO message - default level");
    ESP_LOGD(TAG, "This is a DEBUG message - needs debug level");
    ESP_LOGV(TAG, "This is a VERBOSE message - needs verbose level");
}

void demonstrate_formatted_logging(void)
{
    int   temperature = 25;
    float voltage     = 3.3f;
    const char* status = "OK";

    ESP_LOGI(TAG, "Sensor readings:");
    ESP_LOGI(TAG, "  Temperature: %d°C", temperature);
    ESP_LOGI(TAG, "  Voltage: %.2fV", voltage);
    ESP_LOGI(TAG, "  Status: %s", status);

    // Hex dump example
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    ESP_LOGI(TAG, "Data dump:");
    ESP_LOG_BUFFER_HEX(TAG, data, sizeof(data));
}

void demonstrate_conditional_logging(void)
{
    int error_code = 0;

    if (error_code != 0) {
        ESP_LOGE(TAG, "Error occurred: code %d", error_code);
    } else {
        ESP_LOGI(TAG, "System is running normally");
    }

    // Init NVS (ตัวอย่างการใช้ ESP_ERROR_CHECK)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized successfully");
}

void app_main(void)
{
    // ---- ตั้งค่า Log Level ----
    // เฉพาะ TAG นี้ให้ละเอียดถึงระดับ DEBUG
    esp_log_level_set("LOGGING_DEMO", ESP_LOG_DEBUG);
    // Global (ทุก TAG) ให้แสดงตั้งแต่ INFO ขึ้นไป
    esp_log_level_set("*", ESP_LOG_INFO);

    // ---- System information ----
    ESP_LOGI(TAG, "=== ESP32 Logging Demo ===");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Chip Model (target): %s", CONFIG_IDF_TARGET);

    uint32_t free_heap      = (uint32_t)esp_get_free_heap_size();
    uint32_t min_free_heap  = (uint32_t)esp_get_minimum_free_heap_size();
    ESP_LOGI(TAG, "Free Heap: %" PRIu32 " bytes", free_heap);
    ESP_LOGI(TAG, "Min Free Heap: %" PRIu32 " bytes", min_free_heap);

    // CPU & Flash info
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip cores: %d", chip_info.cores);

    uint32_t flash_size = 0;
    esp_err_t err = esp_flash_get_size(NULL, &flash_size); // NULL = default chip
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Flash size: %" PRIu32 "MB %s",
                 flash_size / (1024 * 1024),
                 (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    } else {
        ESP_LOGE(TAG, "Failed to get flash size (err=0x%x)", err);
    }

    // ---- Demos ----
    ESP_LOGI(TAG, "--- Logging Levels Demo ---");
    demonstrate_logging_levels();

    ESP_LOGI(TAG, "--- Formatted Logging Demo ---");
    demonstrate_formatted_logging();

    ESP_LOGI(TAG, "--- Conditional Logging Demo ---");
    demonstrate_conditional_logging();

    // ---- Main loop ----
    int counter = 0;
    while (1) {
        ESP_LOGI(TAG, "Main loop iteration: %d", counter++);

        if (counter % 10 == 0) {
            uint32_t cur_free = (uint32_t)esp_get_free_heap_size();
            ESP_LOGI(TAG, "Memory status - Free: %" PRIu32 " bytes", cur_free);
        }
        if (counter % 20 == 0) {
            ESP_LOGW(TAG, "Warning: Counter reached %d", counter);
        }
        if (counter > 50) {
            ESP_LOGE(TAG, "Error simulation: Counter exceeded 50! Resetting counter.");
            counter = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // 2 seconds
    }
}
