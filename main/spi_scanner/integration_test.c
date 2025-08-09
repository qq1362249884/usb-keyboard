/**
 * @file integration_test.c
 * @brief spi_scanner 模块使用新 NVS 管理器的集成测试
 * 
 * 这个文件演示了如何在 spi_scanner 模块中使用新的 NVS 管理器
 */

#include "spi_scanner.h"
#include "esp_log.h"

static const char* TAG = "INTEGRATION_TEST";

/**
 * @brief 测试 spi_scanner 模块的 NVS 功能
 */
void test_spi_scanner_nvs_integration(void) {
    ESP_LOGI(TAG, "Starting spi_scanner NVS integration test");
    
    // 测试初始化
    esp_err_t err = nvs_keymap_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "NVS initialized successfully");
    
    // 测试保存按键映射
    uint16_t test_keymap[NUM_KEYS] = {0};
    for (int i = 0; i < NUM_KEYS; i++) {
        test_keymap[i] = 0x04 + i; // A, B, C, ...
    }
    
    err = save_keymap_to_nvs(0, test_keymap);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Saved test keymap to layer 0 successfully");
    } else {
        ESP_LOGE(TAG, "Failed to save test keymap: %s", esp_err_to_name(err));
    }
    
    // 测试加载按键映射
    uint16_t loaded_keymap[NUM_KEYS] = {0};
    err = load_keymap_from_nvs(0, loaded_keymap);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded keymap from layer 0 successfully");
        // 验证加载的数据
        if (memcmp(test_keymap, loaded_keymap, sizeof(test_keymap)) == 0) {
            ESP_LOGI(TAG, "Keymap verification passed");
        } else {
            ESP_LOGE(TAG, "Keymap verification failed");
        }
    } else {
        ESP_LOGE(TAG, "Failed to load test keymap: %s", esp_err_to_name(err));
    }
    
    // 测试重置功能
    err = reset_keymap_to_default(0);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Reset keymap to default successfully");
    } else {
        ESP_LOGE(TAG, "Failed to reset keymap: %s", esp_err_to_name(err));
    }
    
    // 运行原有的测试函数
    test_keymap_config();
    
    // 清理资源
    nvs_keymap_cleanup();
    
    ESP_LOGI(TAG, "spi_scanner NVS integration test completed");
}

/**
 * @brief 演示如何在主程序中使用
 */
void demo_spi_scanner_usage(void) {
    ESP_LOGI(TAG, "=== spi_scanner NVS Manager Demo ===");
    
    // 1. 初始化 SPI 和 NVS
    spi_hid_init();
    nvs_keymap_init();
    
    // 2. 使用按键映射（现在由新的 NVS 管理器管理）
    ESP_LOGI(TAG, "Keymaps are now managed by the new NVS manager");
    ESP_LOGI(TAG, "Layer 0 keymap[0]: 0x%04X", keymaps[0][0]);
    ESP_LOGI(TAG, "Layer 1 keymap[0]: 0x%04X", keymaps[1][0]);
    
    // 3. 修改并保存按键映射
    uint16_t new_keymap[NUM_KEYS];
    memcpy(new_keymap, keymaps[1], sizeof(new_keymap));
    new_keymap[0] = 0x14; // U键
    new_keymap[1] = 0x15; // I键
    new_keymap[2] = 0x16; // O键
    
    esp_err_t err = save_keymap_to_nvs(1, new_keymap);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Modified and saved layer 1 keymap");
    }
    
    // 4. 验证修改
    uint16_t verify_keymap[NUM_KEYS];
    err = load_keymap_from_nvs(1, verify_keymap);
    if (err == ESP_OK && memcmp(new_keymap, verify_keymap, sizeof(new_keymap)) == 0) {
        ESP_LOGI(TAG, "Keymap modification verified successfully");
    }
    
    // 5. 清理资源（在实际应用中，这通常在程序退出时调用）
    nvs_keymap_cleanup();
    
    ESP_LOGI(TAG, "=== Demo completed ===");
}

/**
 * @brief 兼容性测试
 * 确保原有的API调用仍然正常工作
 */
void test_compatibility(void) {
    ESP_LOGI(TAG, "Testing compatibility with original API");
    
    // 这些调用应该与修改前的行为完全一致
    esp_err_t err = nvs_keymap_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Compatibility test failed: nvs_init");
        return;
    }
    
    // 测试保存和加载
    uint16_t test_data[NUM_KEYS] = {0x29, 0x54, 0x55}; // ESC, /, *
    err = save_keymap_to_nvs(0, test_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Compatibility test failed: save_keymap_to_nvs");
        return;
    }
    
    uint16_t loaded_data[NUM_KEYS] = {0};
    err = load_keymap_from_nvs(0, loaded_data);
    if (err != ESP_OK || memcmp(test_data, loaded_data, sizeof(test_data)) != 0) {
        ESP_LOGE(TAG, "Compatibility test failed: load_keymap_from_nvs");
        return;
    }
    
    // 测试重置
    err = reset_keymap_to_default(0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Compatibility test failed: reset_keymap_to_default");
        return;
    }
    
    // 测试原有的测试函数
    test_keymap_config();
    
    nvs_keymap_cleanup();
    
    ESP_LOGI(TAG, "All compatibility tests passed!");
}