#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_keymap.h"
#include "spi_config.h"

static const char *TAG = "KEYMAP_TEST";

/**
 * @brief 按键映射测试任务
 * 这个任务演示如何使用NVS功能来修改、保存和加载按键映射
 */
void keymap_test_task(void *pvParameter) {
    ESP_LOGI(TAG, "Starting keymap test task");

    // 等待系统初始化完成
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // 初始化NVS（如果还没有初始化）
    esp_err_t err = nvs_keymap_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS manager");
        vTaskDelete(NULL);
    }

    // 运行测试函数
    test_keymap_config();

    // 测试持续运行，以便观察效果
    while (1) {
        // 每5秒打印一次当前按键映射
        ESP_LOGI(TAG, "Current keymap for layer 1 (address: %p):", keymaps[1]);
        for (int i = 0; i < NUM_KEYS; i++) {
            ESP_LOGI(TAG, "Key %d: 0x%04X", i, keymaps[1][i]);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

/**
 * @brief 启动按键映射测试
 */
void start_keymap_test(void) {
    xTaskCreate(keymap_test_task, "keymap_test_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Keymap test task created");
}