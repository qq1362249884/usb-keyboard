/**
 * @file c_example_usage.c
 * @brief 在C文件中使用C++ NVS管理器的示例
 * 
 * 这个文件展示了如何在纯C代码中调用C++实现的NVS管理器
 */

#include "c_wrapper.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "C_EXAMPLE";

// 定义按键数量和层数
#define NUM_KEYS 17
#define NUM_LAYERS 2

// 全局NVS管理器句柄
static KeymapNvsManager_t* g_nvs_manager = NULL;

// 默认按键映射
static const uint16_t default_keymaps[NUM_LAYERS][NUM_KEYS] = {
    [0] = { 0x29, 0x54, 0x55, 0x56,  // ESC, /, *, -
             0x67, 0x68, 0x69, 0x57,  // KP7, KP8, KP9, +
             0x64, 0x65, 0x66,        // KP4, KP5, KP6
             0x61, 0x62, 0x63,        // KP1, KP2, KP3
             0x60, 0x63, 0x58 },       // KP0, KP., KP_ENTER
    [1] = { 0x29, 0x54, 0x55, 0x2A,  // ESC, /, *, BACKSPACE
             0x14, 0x1A, 0x08, 0x57,  // Q, W, E, +
             0x04, 0x16, 0x07,        // A, S, D
             0x61, 0x62, 0x63,        // KP1, KP2, KP3
             0x60, 0x63, 0x58 }       // KP0, KP., KP_ENTER
};

// 运行时按键映射
static uint16_t keymaps[NUM_LAYERS][NUM_KEYS] = {0};

/**
 * @brief 初始化NVS管理器
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_nvs_manager_c(void) {
    ESP_LOGI(TAG, "Initializing NVS manager from C code");
    
    // 创建NVS管理器实例
    g_nvs_manager = keymap_nvs_manager_create("keymaps", "keymap_", NUM_KEYS, NUM_LAYERS);
    if (!g_nvs_manager) {
        ESP_LOGE(TAG, "Failed to create NVS manager");
        return ESP_FAIL;
    }
    
    // 初始化管理器
    esp_err_t err = keymap_nvs_manager_init(g_nvs_manager, 
                                           &default_keymaps[0][0], 
                                           &keymaps[0][0]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS manager: %s", 
                 keymap_nvs_manager_get_error_string(g_nvs_manager, err));
        keymap_nvs_manager_destroy(g_nvs_manager);
        g_nvs_manager = NULL;
        return err;
    }
    
    ESP_LOGI(TAG, "NVS manager initialized successfully");
    return ESP_OK;
}

/**
 * @brief 清理NVS管理器
 */
void cleanup_nvs_manager_c(void) {
    if (g_nvs_manager) {
        keymap_nvs_manager_destroy(g_nvs_manager);
        g_nvs_manager = NULL;
        ESP_LOGI(TAG, "NVS manager cleaned up");
    }
}

/**
 * @brief 保存按键映射
 * @param layer 层索引
 * @return ESP_OK 成功，其他失败
 */
esp_err_t save_keymap_c(uint8_t layer) {
    if (!g_nvs_manager) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = keymap_nvs_manager_save(g_nvs_manager, layer, &keymaps[layer][0]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save keymap for layer %d: %s", layer,
                 keymap_nvs_manager_get_error_string(g_nvs_manager, err));
    } else {
        ESP_LOGI(TAG, "Keymap for layer %d saved successfully", layer);
    }
    
    return err;
}

/**
 * @brief 加载按键映射
 * @param layer 层索引
 * @return ESP_OK 成功，其他失败
 */
esp_err_t load_keymap_c(uint8_t layer) {
    if (!g_nvs_manager) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = keymap_nvs_manager_load(g_nvs_manager, layer, &keymaps[layer][0]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load keymap for layer %d: %s", layer,
                 keymap_nvs_manager_get_error_string(g_nvs_manager, err));
    } else {
        ESP_LOGI(TAG, "Keymap for layer %d loaded successfully", layer);
    }
    
    return err;
}

/**
 * @brief 重置按键映射为默认值
 * @param layer 层索引
 * @return ESP_OK 成功，其他失败
 */
esp_err_t reset_keymap_c(uint8_t layer) {
    if (!g_nvs_manager) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = keymap_nvs_manager_reset(g_nvs_manager, layer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset keymap for layer %d: %s", layer,
                 keymap_nvs_manager_get_error_string(g_nvs_manager, err));
    } else {
        ESP_LOGI(TAG, "Keymap for layer %d reset to default successfully", layer);
    }
    
    return err;
}

/**
 * @brief 修改按键映射并保存
 * @param layer 层索引
 * @param key_index 按键索引
 * @param new_keycode 新的键码
 * @return ESP_OK 成功，其他失败
 */
esp_err_t modify_and_save_key_c(uint8_t layer, uint8_t key_index, uint16_t new_keycode) {
    if (layer >= NUM_LAYERS || key_index >= NUM_KEYS) {
        ESP_LOGE(TAG, "Invalid layer %d or key index %d", layer, key_index);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 修改按键映射
    keymaps[layer][key_index] = new_keycode;
    ESP_LOGI(TAG, "Modified key %d in layer %d to 0x%04X", key_index, layer, new_keycode);
    
    // 保存到NVS
    return save_keymap_c(layer);
}

/**
 * @brief 检查按键映射是否存在
 * @param layer 层索引
 * @return 1 存在，0 不存在
 */
int check_keymap_exists_c(uint8_t layer) {
    if (!g_nvs_manager) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return 0;
    }
    
    int exists = keymap_nvs_manager_exists(g_nvs_manager, layer);
    ESP_LOGI(TAG, "Keymap for layer %d exists: %s", layer, exists ? "Yes" : "No");
    return exists;
}

/**
 * @brief 打印按键映射
 * @param layer 层索引
 */
void print_keymap_c(uint8_t layer) {
    if (layer >= NUM_LAYERS) {
        ESP_LOGE(TAG, "Invalid layer %d", layer);
        return;
    }
    
    ESP_LOGI(TAG, "Keymap for layer %d:", layer);
    for (int i = 0; i < NUM_KEYS; i++) {
        ESP_LOGI(TAG, "  Key %2d: 0x%04X", i, keymaps[layer][i]);
    }
}

/**
 * @brief 测试NVS管理器功能
 */
void test_nvs_manager_c(void) {
    ESP_LOGI(TAG, "Starting NVS manager test from C code");
    
    // 初始化
    esp_err_t err = init_nvs_manager_c();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS manager");
        return;
    }
    
    // 打印初始按键映射
    print_keymap_c(1);
    
    // 检查是否存在
    check_keymap_exists_c(1);
    
    // 修改几个按键
    modify_and_save_key_c(1, 0, 0x14);  // U键
    modify_and_save_key_c(1, 1, 0x15);  // I键
    modify_and_save_key_c(1, 2, 0x16);  // O键
    
    // 打印修改后的按键映射
    print_keymap_c(1);
    
    // 重新加载验证
    ESP_LOGI(TAG, "Reloading keymap to verify...");
    err = load_keymap_c(1);
    if (err == ESP_OK) {
        print_keymap_c(1);
    }
    
    // 运行C++的测试函数
    ESP_LOGI(TAG, "Running C++ test function...");
    keymap_nvs_manager_test_config(g_nvs_manager, &keymaps[0][0]);
    
    // 清理
    cleanup_nvs_manager_c();
    
    ESP_LOGI(TAG, "NVS manager test completed");
}

/**
 * @brief 兼容原有接口的NVS初始化函数
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_init_c(void) {
    return init_nvs_manager_c();
}

/**
 * @brief 兼容原有接口的保存按键映射函数
 * @param layer 层索引
 * @param keymap 按键映射数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t save_keymap_to_nvs_c(uint8_t layer, const uint16_t* keymap) {
    if (!g_nvs_manager) {
        esp_err_t err = init_nvs_manager_c();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    // 复制到运行时数组
    memcpy(&keymaps[layer][0], keymap, sizeof(uint16_t) * NUM_KEYS);
    
    return keymap_nvs_manager_save(g_nvs_manager, layer, &keymaps[layer][0]);
}

/**
 * @brief 兼容原有接口的加载按键映射函数
 * @param layer 层索引
 * @param keymap 用于存储按键映射的数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t load_keymap_from_nvs_c(uint8_t layer, uint16_t* keymap) {
    if (!g_nvs_manager) {
        esp_err_t err = init_nvs_manager_c();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    esp_err_t err = keymap_nvs_manager_load(g_nvs_manager, layer, &keymaps[layer][0]);
    if (err == ESP_OK) {
        // 复制到输出数组
        memcpy(keymap, &keymaps[layer][0], sizeof(uint16_t) * NUM_KEYS);
    }
    
    return err;
}

/**
 * @brief 兼容原有接口的重置按键映射函数
 * @param layer 层索引
 * @return ESP_OK 成功，其他失败
 */
esp_err_t reset_keymap_to_default_c(uint8_t layer) {
    if (!g_nvs_manager) {
        esp_err_t err = init_nvs_manager_c();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    return keymap_nvs_manager_reset(g_nvs_manager, layer);
}

/**
 * @brief 兼容原有接口的测试函数
 */
void test_keymap_config_c(void) {
    if (!g_nvs_manager) {
        esp_err_t err = init_nvs_manager_c();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize NVS manager");
            return;
        }
    }
    
    keymap_nvs_manager_test_config(g_nvs_manager, &keymaps[0][0]);
}