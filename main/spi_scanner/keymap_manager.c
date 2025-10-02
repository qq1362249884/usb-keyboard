#include "keymap_manager.h"
#include "nvs_manager/unified_nvs_manager.h"

#ifdef __cplusplus
extern "C" {
#endif


static const char *TAG_NVS = "NVS_KEYMAP";

// 默认按键映射
// 注意：层0是不可修改的默认映射，层1-6是可自定义的映射
uint16_t keymaps[TOTAL_LAYERS][NUM_KEYS] = {
    // 层0 - 不可修改的默认映射
    [0] ={  KC_ESC , KC_KP_SLASH, KC_KP_ASTERISK, KC_KP_MINUS, 
            KC_KP_7, KC_KP_8, KC_KP_9, KC_KP_PLUS, 
            KC_KP_4, KC_KP_5, KC_KP_6, 
            KC_KP_1, KC_KP_2, KC_KP_3, 
            KC_KP_0, KC_KP_DOT, KC_KP_ENTER},
    // 层1-6 - 可自定义的映射（初始化为空）
    [1] ={},
    [2] ={},
    [3] ={},
    [4] ={},
    [5] ={},
    [6] ={}
};

// 全局统一NVS管理器句柄（使用外部管理器）
static unified_nvs_manager_t* g_nvs_manager = NULL;

/**
 * @brief 设置NVS管理器实例
 * @param manager 外部NVS管理器实例
 */
void set_nvs_manager(unified_nvs_manager_t* manager) {
    g_nvs_manager = manager;
}

/**
 * @brief 初始化NVS并加载按键映射
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t nvs_keymap_init(void) {    
    // 如果NVS管理器已存在，直接使用
    if (g_nvs_manager) {
        ESP_LOGI(TAG_NVS, "Using existing unified NVS manager");
        
        // 从NVS加载所有自定义层(层1-6)的按键映射数据到运行时数组
        // 这样系统重启后，自定义层的数据会自动加载
        for (uint8_t layer = FIRST_CUSTOM_LAYER; layer <= LAST_CUSTOM_LAYER; layer++) {
            esp_err_t err = unified_nvs_load_keymap_layer(g_nvs_manager, layer, &keymaps[layer][0], NUM_KEYS);
            if (err == ESP_OK) {
                ESP_LOGI(TAG_NVS, "Successfully loaded custom keymap (layer %d) from NVS", layer);
            }
        }
        
        return ESP_OK;
    }
    
    // 如果没有外部管理器，创建默认实例
    g_nvs_manager = unified_nvs_manager_create_default();
    if (!g_nvs_manager) {
        ESP_LOGE(TAG_NVS, "Failed to create unified NVS manager");
        return ESP_FAIL;
    }
    
    // 初始化管理器
    esp_err_t err = unified_nvs_manager_init(g_nvs_manager);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Failed to initialize unified NVS manager");
        unified_nvs_manager_destroy(g_nvs_manager);
        g_nvs_manager = NULL;
        return err;
    }
    
    // 从NVS加载所有自定义层(层1-6)的按键映射数据到运行时数组
    // 这样系统重启后，自定义层的数据会自动加载
    for (uint8_t layer = FIRST_CUSTOM_LAYER; layer <= LAST_CUSTOM_LAYER; layer++) {
        err = unified_nvs_load_keymap_layer(g_nvs_manager, layer, &keymaps[layer][0], NUM_KEYS);
        if (err == ESP_OK) {
            ESP_LOGI(TAG_NVS, "Successfully loaded custom keymap (layer %d) from NVS", layer);
        }
    }
    
    ESP_LOGI(TAG_NVS, "Unified NVS manager initialized successfully");
    return ESP_OK;
}

/**
 * @brief 保存按键映射到NVS
 * @param layer 层索引
 * @param keymap 按键映射数组
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t save_keymap_to_nvs(uint8_t layer, const uint16_t *keymap) {
    if (!g_nvs_manager) {
        esp_err_t err = nvs_keymap_init();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    // 复制到运行时数组
    memcpy(&keymaps[layer][0], keymap, sizeof(uint16_t) * NUM_KEYS);
    
    esp_err_t err = unified_nvs_save_keymap_layer(g_nvs_manager, layer, &keymaps[layer][0], NUM_KEYS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Failed to save keymap for layer %d", layer);
    } else {
        ESP_LOGI(TAG_NVS, "Saved keymap for layer %d successfully", layer);
    }
    
    return err;
}

/**
 * @brief 从NVS加载按键映射
 * @param layer 层索引
 * @param keymap 用于存储按键映射的数组
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t load_keymap_from_nvs(uint8_t layer, uint16_t *keymap) {
    if (!g_nvs_manager) {
        esp_err_t err = nvs_keymap_init();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    esp_err_t err = unified_nvs_load_keymap_layer(g_nvs_manager, layer, &keymaps[layer][0], NUM_KEYS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Failed to load keymap for layer %d", layer);
    } else {
        ESP_LOGI(TAG_NVS, "Loaded keymap for layer %d successfully", layer);
    }
    
    // 无论加载是否成功，都将当前映射复制到输出数组
    // 这样即使加载失败，也能返回内存中的默认映射
    memcpy(keymap, &keymaps[layer][0], sizeof(uint16_t) * NUM_KEYS);
    
    return err;
}

/**
 * @brief 将按键映射重置为默认值
 * @param layer 层索引
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t reset_keymap_to_default(uint8_t layer) {
    if (!g_nvs_manager) {
        esp_err_t err = nvs_keymap_init();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    // 使用新的统一NVS管理器删除按键映射层数据
    char key[32];
    snprintf(key, sizeof(key), "layer_%d", layer);
    esp_err_t err = unified_nvs_manager_erase(g_nvs_manager, NVS_NAMESPACE_KEYMAP, key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Failed to reset keymap for layer %d", layer);
    } else {
        ESP_LOGI(TAG_NVS, "Reset keymap for layer %d to default successfully", layer);
    }
    
    return err;
}

/**
 * @brief 保存单个按键到NVS
 * @param layer 层索引
 * @param key_index 按键索引
 * @param key_code 键码
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t save_single_key_to_nvs(uint8_t layer, uint8_t key_index, uint16_t key_code) {
    if (!g_nvs_manager) {
        esp_err_t err = nvs_keymap_init();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    // 检查索引是否有效
    if (key_index >= NUM_KEYS) {
        ESP_LOGE(TAG_NVS, "Invalid key index: %d (max: %d)", key_index, NUM_KEYS - 1);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 更新运行时数组中的单个按键
    keymaps[layer][key_index] = key_code;
    
    // 保存整个映射到NVS
    esp_err_t err = unified_nvs_save_keymap_layer(g_nvs_manager, layer, &keymaps[layer][0], NUM_KEYS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Failed to save single key for layer %d, index %d", layer, key_index);
    } else {
        ESP_LOGI(TAG_NVS, "Saved single key for layer %d, index %d, code %d successfully", layer, key_index, key_code);
    }
    
    return err;
}



/**
 * @brief 清理NVS管理器资源
 * 这个函数应该在程序退出时调用，释放NVS管理器资源
 */
void nvs_keymap_cleanup(void) {
    if (g_nvs_manager) {
        unified_nvs_manager_destroy(g_nvs_manager);
        g_nvs_manager = NULL;
        ESP_LOGI(TAG_NVS, "Unified NVS manager cleaned up");
    }
}



/**
 * @brief 检查是否为组合键
 * @param keycode 键码
 * @return true 是组合键，false 不是组合键
 */
bool is_combo_key(uint16_t keycode) {
    return (keycode & KEY_COMBO_FLAG) == KEY_COMBO_FLAG;
}

/**
 * @brief 获取组合键的基础键码
 * @param keycode 组合键码
 * @return 基础键码
 */
uint16_t get_base_key(uint16_t keycode) {
    return keycode & KEY_BASE_MASK;
}

/**
 * @brief 获取组合键的修饰键掩码
 * @param keycode 组合键码
 * @return 修饰键掩码
 */
uint16_t get_modifier_mask(uint16_t keycode) {
    return keycode & KEY_MODIFIER_MASK;
}

/**
 * @brief 创建组合键
 * @param base_key 基础键码
 * @param modifier_mask 修饰键掩码
 * @return 组合键码
 */
uint16_t create_combo_key(uint16_t base_key, uint16_t modifier_mask) {
    return KEY_COMBO_FLAG | (base_key & KEY_BASE_MASK) | (modifier_mask & KEY_MODIFIER_MASK);
}

#ifdef __cplusplus
}
#endif