#include "keymap_nvs_manager.h"
#include "nvs_manager.h"
#include "esp_log.h"
#include "string.h"

// 定义默认的命名空间和键名前缀
#define KEYMAP_DEFAULT_NAMESPACE "keymaps"
#define KEYMAP_DEFAULT_PREFIX "keymap_"

// 包装器结构定义
struct KeymapNvsManagerWrapper {
    NvsCommonManager_t* common_manager;
    const char* key_prefix;
    uint8_t num_keys;
    uint8_t num_layers;
    const uint16_t* default_keymaps;
};

static const char* TAG = "KEYMAP_NVS";

/**
 * @brief 生成层对应的键名
 */
static void generate_key_name(const char* key_prefix, uint8_t layer, char* key_buffer, size_t buffer_size) {
    snprintf(key_buffer, buffer_size, "%s%d", key_prefix, layer);
}

/**
 * @brief 检查层索引是否有效
 */
static bool is_valid_layer(uint8_t layer, uint8_t num_layers) {
    return layer < num_layers;
}

/**
 * @brief 创建按键映射NVS管理器实例
 */
KeymapNvsManager_t* keymap_nvs_manager_create(const char* namespace_name, 
                                             const char* key_prefix, 
                                             uint8_t num_keys, 
                                             uint8_t num_layers, 
                                             const uint16_t* default_keymaps) {
    // 使用默认值
    const char* ns = namespace_name ? namespace_name : KEYMAP_DEFAULT_NAMESPACE;
    const char* prefix = key_prefix ? key_prefix : KEYMAP_DEFAULT_PREFIX;
    
    KeymapNvsManager_t* manager = malloc(sizeof(KeymapNvsManager_t));
    if (!manager) {
        ESP_LOGE(TAG, "Failed to allocate memory for keymap manager");
        return NULL;
    }
    
    // 创建通用NVS管理器
    manager->common_manager = nvs_common_manager_create(ns);
    if (!manager->common_manager) {
        ESP_LOGE(TAG, "Failed to create common NVS manager");
        free(manager);
        return NULL;
    }
    
    // 设置其他属性
    manager->key_prefix = prefix;
    manager->num_keys = num_keys;
    manager->num_layers = num_layers;
    manager->default_keymaps = default_keymaps;
    
    return manager;
}

/**
 * @brief 销毁按键映射NVS管理器实例
 */
void keymap_nvs_manager_destroy(KeymapNvsManager_t* manager) {
    if (!manager) {
        return;
    }
    
    if (manager->common_manager) {
        nvs_common_manager_destroy(manager->common_manager);
    }
    
    free(manager);
}

/**
 * @brief 初始化按键映射NVS管理器
 */
esp_err_t keymap_nvs_manager_init(KeymapNvsManager_t* manager) {
    if (!manager || !manager->common_manager) {
        ESP_LOGE(TAG, "Invalid manager handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 初始化通用管理器
    esp_err_t err = nvs_common_manager_init(manager->common_manager);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize common NVS manager: %s", esp_err_to_name(err));
        return err;
    }
    
    // 打开NVS命名空间以便后续操作
    NvsBaseManager_t* base_manager = ((struct NvsCommonManagerWrapper*)manager->common_manager)->base_manager;
    err = nvs_base_open(base_manager, false); // false表示可读写模式
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace during initialization: %s", 
                 esp_err_to_name(err));
        // 即使打开失败，初始化仍算成功，因为可以在后续操作中重试打开
    }
    
    return ESP_OK;
}

/**
 * @brief 保存指定层的按键映射
 */
esp_err_t keymap_nvs_manager_save(KeymapNvsManager_t* manager, 
                                 uint8_t layer, 
                                 const uint16_t* keymap) {
    if (!manager || !manager->common_manager || !keymap) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_valid_layer(layer, manager->num_layers)) {
        ESP_LOGE(TAG, "Invalid layer index: %d (max: %d)", layer, manager->num_layers - 1);
        return ESP_ERR_INVALID_ARG;
    }
    
    char key[32];
    generate_key_name(manager->key_prefix, layer, key, sizeof(key));
    
    // 获取基础管理器
    NvsCommonManager_t* common_wrapper = manager->common_manager;
    NvsBaseManager_t* base_manager = common_wrapper->base_manager;
    
    // 确保NVS命名空间已打开（使用读写模式）
    esp_err_t open_err = nvs_base_open(base_manager, false);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(open_err));
        return open_err;
    }
    
    // 保存数据
    esp_err_t err = nvs_common_manager_save_blob(manager->common_manager, 
                                                key, 
                                                keymap, 
                                                sizeof(uint16_t) * manager->num_keys);
    
    if (err == ESP_OK) {
        // 显式提交更改以确保数据持久化
        esp_err_t commit_err = nvs_base_commit(base_manager);
        if (commit_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS changes for layer %d: %s", 
                     layer, esp_err_to_name(commit_err));
            return commit_err;
        }
    } else {
        ESP_LOGE(TAG, "Failed to save keymap for layer %d: %s", 
                 layer, esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief 加载指定层的按键映射
 */
esp_err_t keymap_nvs_manager_load(KeymapNvsManager_t* manager, 
                                 uint8_t layer, 
                                 uint16_t* keymap) {
    if (!manager || !manager->common_manager || !keymap) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_valid_layer(layer, manager->num_layers)) {
        ESP_LOGE(TAG, "Invalid layer index: %d (max: %d)", layer, manager->num_layers - 1);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 获取基础管理器
    NvsCommonManager_t* common_wrapper = manager->common_manager;
    NvsBaseManager_t* base_manager = ((struct NvsCommonManagerWrapper*)common_wrapper)->base_manager;
    
    // 确保NVS命名空间已打开（使用只读模式）
    esp_err_t open_err = nvs_base_open(base_manager, true);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(open_err));
        
        // 即使打开失败，如果有默认映射也返回默认值
        if (manager->default_keymaps != NULL) {
            memcpy(keymap, 
                  &manager->default_keymaps[layer * manager->num_keys], 
                  sizeof(uint16_t) * manager->num_keys);
            ESP_LOGW(TAG, "NVS namespace open failed, using default keymap for layer %d", layer);
            return ESP_OK;
        }
        
        return open_err;
    }
    
    char key[32];
    generate_key_name(manager->key_prefix, layer, key, sizeof(key));
    
    size_t size = sizeof(uint16_t) * manager->num_keys;
    // 加载数据
    esp_err_t err = nvs_common_manager_load_blob(manager->common_manager, 
                                                key, 
                                                keymap, 
                                                &size);
    
    if (err == ESP_OK) {
        // 加载成功
    } else if (err == ESP_ERR_NOT_FOUND) {
        // 如果找不到，使用默认值
        if (manager->default_keymaps != NULL) {
            memcpy(keymap, 
                  &manager->default_keymaps[layer * manager->num_keys], 
                  sizeof(uint16_t) * manager->num_keys);
            ESP_LOGW(TAG, "Keymap for layer %d not found, using default", layer);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Keymap for layer %d not found and no default available", layer);
        }
    } else {
        ESP_LOGE(TAG, "Failed to load keymap for layer %d: %s", 
                 layer, esp_err_to_name(err));
        
        // 加载失败时也尝试使用默认映射
        if (manager->default_keymaps != NULL) {
            memcpy(keymap, 
                  &manager->default_keymaps[layer * manager->num_keys], 
                  sizeof(uint16_t) * manager->num_keys);
            ESP_LOGW(TAG, "Using default keymap for layer %d due to load failure", layer);
            // 仍然返回原始错误码，以便调用者知道加载失败但使用了默认值
        }
    }
    
    return err;
}

/**
 * @brief 重置指定层的按键映射为默认值
 */
esp_err_t keymap_nvs_manager_reset(KeymapNvsManager_t* manager, uint8_t layer) {
    if (!manager || !manager->common_manager) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_valid_layer(layer, manager->num_layers)) {
        ESP_LOGE(TAG, "Invalid layer index: %d (max: %d)", layer, manager->num_layers - 1);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (manager->default_keymaps == NULL) {
        ESP_LOGE(TAG, "Default keymaps not available");
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // 获取默认映射的指针
    const uint16_t* default_keymap = &manager->default_keymaps[layer * manager->num_keys];
    
    // 保存默认映射到NVS
    esp_err_t err = keymap_nvs_manager_save(manager, layer, default_keymap);
    
    if (err == ESP_OK) {
        // 重置成功
    } else {
        ESP_LOGE(TAG, "Failed to reset keymap for layer %d to default: %s", 
                 layer, esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief 重置所有层的按键映射为默认值
 */
esp_err_t keymap_nvs_manager_reset_all_to_default(KeymapNvsManager_t* manager) {
    if (!manager || !manager->common_manager) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!manager->default_keymaps) {
        ESP_LOGE(TAG, "No default keymap available");
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // 保存所有默认映射到NVS
    esp_err_t err = keymap_nvs_manager_save_all(manager, manager->default_keymaps);
    
    if (err == ESP_OK) {
        // 全部重置成功
    } else {
        ESP_LOGE(TAG, "Failed to reset all keymap layers to default: %s", 
                 esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief 保存所有层的按键映射
 */
esp_err_t keymap_nvs_manager_save_all(KeymapNvsManager_t* manager, const uint16_t* keymaps) {
    if (!manager || !manager->common_manager || !keymaps) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t overall_err = ESP_OK;
    uint8_t success_count = 0;
    
    // 获取基础管理器
    NvsCommonManager_t* common_wrapper = manager->common_manager;
    NvsBaseManager_t* base_manager = common_wrapper->base_manager;
    
    // 确保NVS命名空间已打开（使用读写模式）
    esp_err_t open_err = nvs_base_open(base_manager, false);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(open_err));
        return open_err;
    }
    
    // 批量保存所有层
    for (uint8_t layer = 0; layer < manager->num_layers; layer++) {
        char key[32];
        generate_key_name(manager->key_prefix, layer, key, sizeof(key));
        
        esp_err_t err = nvs_common_manager_save_blob(manager->common_manager, 
                                                    key, 
                                                    &keymaps[layer * manager->num_keys], 
                                                    sizeof(uint16_t) * manager->num_keys);
        
        if (err == ESP_OK) {
            success_count++;
        } else {
            ESP_LOGE(TAG, "Failed to save keymap for layer %d: %s", 
                     layer, esp_err_to_name(err));
            overall_err = err;
        }
    }
    
    // 统一提交所有更改以提高效率
    if (success_count > 0) {
        esp_err_t commit_err = nvs_base_commit(base_manager);
        if (commit_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit batch NVS changes: %s", 
                     esp_err_to_name(commit_err));
            return commit_err;
        }
    }
    
    return overall_err;
}

/**
 * @brief 加载所有层的按键映射
 */
esp_err_t keymap_nvs_manager_load_all(KeymapNvsManager_t* manager, uint16_t* keymaps) {
    if (!manager || !manager->common_manager || !keymaps) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t overall_err = ESP_OK;
    uint8_t success_count = 0;
    uint8_t default_count = 0;
    
    // 获取基础管理器
    NvsCommonManager_t* common_wrapper = manager->common_manager;
    NvsBaseManager_t* base_manager = ((struct NvsCommonManagerWrapper*)common_wrapper)->base_manager;
    
    // 确保NVS命名空间已打开（使用只读模式）
    esp_err_t open_err = nvs_base_open(base_manager, true);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(open_err));
        
        // 如果打开失败，尝试使用所有默认映射
        if (manager->default_keymaps != NULL) {
            memcpy(keymaps, 
                  manager->default_keymaps, 
                  sizeof(uint16_t) * manager->num_layers * manager->num_keys);
            ESP_LOGW(TAG, "NVS namespace open failed, using all default keymaps");
            return ESP_OK;
        }
        
        return open_err;
    }
    
    // 加载所有层的按键映射
    for (uint8_t layer = 0; layer < manager->num_layers; layer++) {
        char key[32];
        generate_key_name(manager->key_prefix, layer, key, sizeof(key));
        
        size_t size = sizeof(uint16_t) * manager->num_keys;
        esp_err_t err = nvs_common_manager_load_blob(manager->common_manager, 
                                                    key, 
                                                    &keymaps[layer * manager->num_keys], 
                                                    &size);
        
        if (err == ESP_OK) {
            success_count++;
        } else if (err == ESP_ERR_NOT_FOUND || err != ESP_OK) {
            // 如果没有找到或加载失败，使用默认映射
            if (manager->default_keymaps != NULL) {
                memcpy(&keymaps[layer * manager->num_keys], 
                      &manager->default_keymaps[layer * manager->num_keys], 
                      sizeof(uint16_t) * manager->num_keys);
                default_count++;
                if (err == ESP_ERR_NOT_FOUND) {
                    ESP_LOGW(TAG, "Keymap for layer %d not found, using default", layer);
                } else {
                    ESP_LOGW(TAG, "Failed to load keymap for layer %d: %s, using default", 
                             layer, esp_err_to_name(err));
                }
            } else {
                ESP_LOGE(TAG, "Keymap for layer %d not found and no default available", layer);
                overall_err = err;
            }
        }
    }
    
    // 加载完成
    
    return overall_err;
}

/**
 * @brief 检查指定层的按键映射是否存在
 */
int keymap_nvs_manager_exists(KeymapNvsManager_t* manager, uint8_t layer) {
    if (!manager || !manager->common_manager) {
        ESP_LOGE(TAG, "Invalid manager handle");
        return 0;
    }
    
    if (!is_valid_layer(layer, manager->num_layers)) {
        ESP_LOGE(TAG, "Invalid layer index: %d", layer);
        return 0;
    }
    
    char key[32];
    generate_key_name(manager->key_prefix, layer, key, sizeof(key));
    
    return nvs_common_manager_exists(manager->common_manager, key);
}

/**
 * @brief 获取按键数量
 */
uint8_t keymap_nvs_manager_get_num_keys(KeymapNvsManager_t* manager) {
    if (!manager) {
        return 0;
    }
    return manager->num_keys;
}

/**
 * @brief 获取层数
 */
uint8_t keymap_nvs_manager_get_num_layers(KeymapNvsManager_t* manager) {
    if (!manager) {
        return 0;
    }
    return manager->num_layers;
}

/**
 * @brief 测试按键映射配置功能
 */
// 测试函数已禁用，移除了所有调试打印
// void keymap_nvs_manager_test_config(KeymapNvsManager_t* manager, uint16_t* keymaps) {
//     // 实现内容已移除
// }