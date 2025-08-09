#include "esp_log.h"

// 示例：如何在spi_scanner模块中使用KeymapNvsManager

extern "C" {
    #include "spi_scanner.h"
}

#include "keymap_nvs_manager.h"

static const char* TAG = "NVS_EXAMPLE";

// 全局NVS管理器实例
static KeymapNvsManager* g_keymap_nvs_manager = nullptr;

/**
 * @brief 初始化按键映射NVS管理器
 * @param default_keymaps 默认按键映射数组
 * @param keymaps 运行时按键映射数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_keymap_nvs_manager(const uint16_t* default_keymaps, uint16_t* keymaps) {
    if (g_keymap_nvs_manager != nullptr) {
        ESP_LOGW(TAG, "Keymap NVS manager already initialized");
        return ESP_OK;
    }
    
    // 创建NVS管理器实例
    g_keymap_nvs_manager = new KeymapNvsManager("keymaps", "keymap_", NUM_KEYS, 2);
    
    // 初始化管理器
    esp_err_t err = g_keymap_nvs_manager->init_with_keymaps(default_keymaps, keymaps);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize keymap NVS manager");
        delete g_keymap_nvs_manager;
        g_keymap_nvs_manager = nullptr;
        return err;
    }
    
    ESP_LOGI(TAG, "Keymap NVS manager initialized successfully");
    return ESP_OK;
}

/**
 * @brief 清理按键映射NVS管理器
 */
void cleanup_keymap_nvs_manager() {
    if (g_keymap_nvs_manager != nullptr) {
        delete g_keymap_nvs_manager;
        g_keymap_nvs_manager = nullptr;
        ESP_LOGI(TAG, "Keymap NVS manager cleaned up");
    }
}

/**
 * @brief 保存按键映射到NVS（兼容原接口）
 * @param layer 层索引
 * @param keymap 按键映射数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t save_keymap_to_nvs(uint8_t layer, const uint16_t* keymap) {
    if (g_keymap_nvs_manager == nullptr) {
        ESP_LOGE(TAG, "Keymap NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    return g_keymap_nvs_manager->save_keymap(layer, keymap);
}

/**
 * @brief 从NVS加载按键映射（兼容原接口）
 * @param layer 层索引
 * @param keymap 用于存储按键映射的数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t load_keymap_from_nvs(uint8_t layer, uint16_t* keymap) {
    if (g_keymap_nvs_manager == nullptr) {
        ESP_LOGE(TAG, "Keymap NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    return g_keymap_nvs_manager->load_keymap(layer, keymap);
}

/**
 * @brief 重置按键映射为默认值（兼容原接口）
 * @param layer 层索引
 * @return ESP_OK 成功，其他失败
 */
esp_err_t reset_keymap_to_default(uint8_t layer) {
    if (g_keymap_nvs_manager == nullptr) {
        ESP_LOGE(TAG, "Keymap NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    return g_keymap_nvs_manager->reset_keymap_to_default(layer);
}

/**
 * @brief 测试按键映射配置（兼容原接口）
 */
void test_keymap_config(void) {
    if (g_keymap_nvs_manager == nullptr) {
        ESP_LOGE(TAG, "Keymap NVS manager not initialized");
        return;
    }
    
    // 注意：这里需要传入运行时keymaps数组的指针
    extern uint16_t keymaps[2][NUM_KEYS];
    g_keymap_nvs_manager->test_keymap_config(&keymaps[0][0]);
}

/**
 * @brief 初始化NVS并加载按键映射（兼容原接口）
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_init(void) {
    // 注意：这里需要传入default_keymaps和keymaps数组
    extern uint16_t keymaps[2][NUM_KEYS];
    extern const uint16_t default_keymaps[][NUM_KEYS];
    
    return init_keymap_nvs_manager(&default_keymaps[0][0], &keymaps[0][0]);
}

/**
 * @brief 示例：使用NVS管理器的高级功能
 */
void example_advanced_usage() {
    if (g_keymap_nvs_manager == nullptr) {
        ESP_LOGE(TAG, "Keymap NVS manager not initialized");
        return;
    }
    
    // 检查特定层的按键映射是否存在
    bool exists = g_keymap_nvs_manager->keymap_exists(1);
    ESP_LOGI(TAG, "Keymap for layer 1 exists: %s", exists ? "Yes" : "No");
    
    // 获取配置信息
    ESP_LOGI(TAG, "Number of keys: %d", g_keymap_nvs_manager->get_num_keys());
    ESP_LOGI(TAG, "Number of layers: %d", g_keymap_nvs_manager->get_num_layers());
    
    // 批量保存所有层的按键映射
    extern uint16_t keymaps[2][NUM_KEYS];
    esp_err_t err = g_keymap_nvs_manager->save_all_keymaps(&keymaps[0][0]);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "All keymaps saved successfully");
    }
}