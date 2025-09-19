#include "menu_nvs_manager.h"
#include "nvs_manager.h"
#include "esp_log.h"
#include "string.h"
#include <inttypes.h>

// 定义默认的命名空间和键名
#define MENU_DEFAULT_NAMESPACE "menu_config"
#define KEY_CURRENT_LAYER "current_layer"
#define KEY_WS2812_STATE "ws2812_state"
#define KEY_WIFI_STATE "wifi_state"
#define KEY_WIFI_MODE "wifi_mode"

// 包装器结构定义
struct MenuNvsManagerWrapper {
    NvsCommonManager_t* common_manager;
    uint8_t default_layer;
    bool default_ws2812_state;
    bool default_wifi_state;
};

static const char* TAG = "MENU_NVS";

/**
 * @brief 创建菜单NVS管理器实例（扩展版本，支持WiFi状态默认值）
 */
MenuNvsManager_t* menu_nvs_manager_create_ext(const char* namespace_name, 
                                             uint8_t default_layer, 
                                             bool default_ws2812_state,
                                             bool default_wifi_state) {
    // 使用默认值
    const char* ns = namespace_name ? namespace_name : MENU_DEFAULT_NAMESPACE;
    
    MenuNvsManager_t* manager = malloc(sizeof(MenuNvsManager_t));
    if (!manager) {
        ESP_LOGE(TAG, "Failed to allocate memory for menu manager");
        return NULL;
    }
    
    // 创建通用NVS管理器
    manager->common_manager = nvs_common_manager_create(ns);
    if (!manager->common_manager) {
        ESP_LOGE(TAG, "Failed to create common NVS manager");
        free(manager);
        return NULL;
    }
    
    // 设置默认值
    manager->default_layer = default_layer;
    manager->default_ws2812_state = default_ws2812_state;
    manager->default_wifi_state = default_wifi_state;
    
    ESP_LOGI(TAG, "Menu NVS manager created successfully");
    return manager;
}

/**
 * @brief 销毁菜单NVS管理器实例
 */
void menu_nvs_manager_destroy(MenuNvsManager_t* manager) {
    if (!manager) {
        return;
    }
    
    if (manager->common_manager) {
        nvs_common_manager_destroy(manager->common_manager);
    }
    
    free(manager);
    ESP_LOGI(TAG, "Menu NVS manager destroyed");
}

/**
 * @brief 初始化菜单NVS管理器
 */
esp_err_t menu_nvs_manager_init(MenuNvsManager_t* manager) {
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
    
    ESP_LOGI(TAG, "Menu NVS manager initialized successfully");
    return ESP_OK;
}

/**
 * @brief 保存当前映射层到NVS
 */
esp_err_t menu_nvs_manager_save_current_layer(MenuNvsManager_t* manager, uint8_t layer) {
    if (!manager || !manager->common_manager) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
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
    esp_err_t err = nvs_common_manager_save_u32(manager->common_manager, 
                                              KEY_CURRENT_LAYER, 
                                              (uint32_t)layer);
    
    if (err == ESP_OK) {
        // 显式提交更改以确保数据持久化
        esp_err_t commit_err = nvs_base_commit(base_manager);
        if (commit_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS changes for current layer: %s", 
                     esp_err_to_name(commit_err));
            return commit_err;
        }
        
        ESP_LOGI(TAG, "Saved and committed current layer %d successfully", layer);
    } else {
        ESP_LOGE(TAG, "Failed to save current layer: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief 从NVS加载当前映射层
 */
esp_err_t menu_nvs_manager_load_current_layer(MenuNvsManager_t* manager, uint8_t* layer) {
    if (!manager || !manager->common_manager || !layer) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 获取基础管理器
    NvsCommonManager_t* common_wrapper = manager->common_manager;
    NvsBaseManager_t* base_manager = common_wrapper->base_manager;
    
    // 确保NVS命名空间已打开（使用只读模式）
    esp_err_t open_err = nvs_base_open(base_manager, true);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(open_err));
        
        // 打开失败时使用默认值
        *layer = manager->default_layer;
        ESP_LOGW(TAG, "NVS namespace open failed, using default layer %d", *layer);
        return ESP_OK;
    }
    
    // 加载数据
    uint32_t layer_u32 = 0;
    esp_err_t err = nvs_common_manager_load_u32(manager->common_manager, 
                                              KEY_CURRENT_LAYER, 
                                              &layer_u32);
    
    if (err == ESP_OK) {
        *layer = (uint8_t)layer_u32;
        ESP_LOGI(TAG, "Loaded current layer %d successfully", *layer);
    } else if (err == ESP_ERR_NOT_FOUND) {
        // 如果找不到，使用默认值
        *layer = manager->default_layer;
        ESP_LOGW(TAG, "Current layer not found, using default %d", *layer);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to load current layer: %s", esp_err_to_name(err));
        
        // 加载失败时也尝试使用默认值
        *layer = manager->default_layer;
        ESP_LOGW(TAG, "Using default layer %d due to load failure", *layer);
        // 仍然返回原始错误码，以便调用者知道加载失败但使用了默认值
    }
    
    return err;
}

/**
 * @brief 保存WiFi状态到NVS
 */
esp_err_t menu_nvs_manager_save_wifi_state(MenuNvsManager_t* manager, bool state) {
    if (!manager || !manager->common_manager) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
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
    esp_err_t err = nvs_common_manager_save_bool(manager->common_manager, 
                                               KEY_WIFI_STATE, 
                                               state);
    
    if (err == ESP_OK) {
        // 显式提交更改以确保数据持久化
        esp_err_t commit_err = nvs_base_commit(base_manager);
        if (commit_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS changes for WiFi state: %s", 
                     esp_err_to_name(commit_err));
            return commit_err;
        }
        
        ESP_LOGI(TAG, "Saved and committed WiFi state %d successfully", state);
    } else {
        ESP_LOGE(TAG, "Failed to save WiFi state: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief 从NVS加载WiFi状态
 */
esp_err_t menu_nvs_manager_load_wifi_state(MenuNvsManager_t* manager, bool* state) {
    if (!manager || !manager->common_manager || !state) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 获取基础管理器
    NvsCommonManager_t* common_wrapper = manager->common_manager;
    NvsBaseManager_t* base_manager = ((struct NvsCommonManagerWrapper*)common_wrapper)->base_manager;
    
    // 确保NVS命名空间已打开（使用只读模式）
    esp_err_t open_err = nvs_base_open(base_manager, true);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(open_err));
        
        // 打开失败时使用默认值
        *state = manager->default_wifi_state;
        ESP_LOGW(TAG, "NVS namespace open failed, using default WiFi state %d", *state);
        return ESP_OK;
    }
    
    // 加载数据
    esp_err_t err = nvs_common_manager_load_bool(manager->common_manager, 
                                               KEY_WIFI_STATE, 
                                               state);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded WiFi state %d successfully", *state);
    } else if (err == ESP_ERR_NOT_FOUND) {
        // 如果找不到，使用默认值
        *state = manager->default_wifi_state;
        ESP_LOGW(TAG, "WiFi state not found, using default %d", *state);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to load WiFi state: %s", esp_err_to_name(err));
        
        // 加载失败时也尝试使用默认值
        *state = manager->default_wifi_state;
        ESP_LOGW(TAG, "Using default WiFi state %d due to load failure", *state);
        // 仍然返回原始错误码，以便调用者知道加载失败但使用了默认值
    }
    
    return err;
}

/**
 * @brief 保存WiFi模式到NVS
 */
esp_err_t menu_nvs_manager_save_wifi_mode(MenuNvsManager_t* manager, wifi_mode_t mode) {
    if (!manager || !manager->common_manager) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
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
    esp_err_t err = nvs_common_manager_save_u32(manager->common_manager, 
                                               KEY_WIFI_MODE, 
                                               (uint32_t)mode);
    
    if (err == ESP_OK) {
        // 显式提交更改以确保数据持久化
        esp_err_t commit_err = nvs_base_commit(base_manager);
        if (commit_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS changes for WiFi mode: %s", 
                     esp_err_to_name(commit_err));
            return commit_err;
        }
        
        ESP_LOGI(TAG, "Saved and committed WiFi mode %d successfully", mode);
    } else {
        ESP_LOGE(TAG, "Failed to save WiFi mode: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief 从NVS加载WiFi模式
 */
esp_err_t menu_nvs_manager_load_wifi_mode(MenuNvsManager_t* manager, wifi_mode_t* mode) {
    if (!manager || !manager->common_manager || !mode) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 获取基础管理器
    NvsCommonManager_t* common_wrapper = manager->common_manager;
    NvsBaseManager_t* base_manager = ((struct NvsCommonManagerWrapper*)common_wrapper)->base_manager;
    
    // 确保NVS命名空间已打开（使用只读模式）
    esp_err_t open_err = nvs_base_open(base_manager, true);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(open_err));
        
        // 打开失败时使用默认值
        *mode = WIFI_MODE_NULL;
        ESP_LOGW(TAG, "NVS namespace open failed, using default WiFi mode %d", *mode);
        return ESP_OK;
    }
    
    // 加载数据
    uint32_t mode_u32 = 0;
    esp_err_t err = nvs_common_manager_load_u32(manager->common_manager, 
                                              KEY_WIFI_MODE, 
                                              &mode_u32);
    
    if (err == ESP_OK) {
        *mode = (wifi_mode_t)mode_u32;
        ESP_LOGI(TAG, "Loaded WiFi mode %d successfully", *mode);
    } else if (err == ESP_ERR_NOT_FOUND) {
        // 如果找不到，使用默认值
        *mode = WIFI_MODE_NULL;
        ESP_LOGW(TAG, "WiFi mode not found, using default %d", *mode);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to load WiFi mode: %s", esp_err_to_name(err));
        
        // 加载失败时也尝试使用默认值
        *mode = WIFI_MODE_NULL;
        ESP_LOGW(TAG, "Using default WiFi mode %d due to load failure", *mode);
        // 仍然返回原始错误码，以便调用者知道加载失败但使用了默认值
    }
    
    return err;
}

/**
 * @brief 保存WS2812状态到NVS
 */
esp_err_t menu_nvs_manager_save_ws2812_state(MenuNvsManager_t* manager, bool state) {
    if (!manager || !manager->common_manager) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
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
    esp_err_t err = nvs_common_manager_save_bool(manager->common_manager, 
                                               KEY_WS2812_STATE, 
                                               state);
    
    if (err == ESP_OK) {
        // 显式提交更改以确保数据持久化
        esp_err_t commit_err = nvs_base_commit(base_manager);
        if (commit_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS changes for WS2812 state: %s", 
                     esp_err_to_name(commit_err));
            return commit_err;
        }
        
        ESP_LOGI(TAG, "Saved and committed WS2812 state %d successfully", state);
    } else {
        ESP_LOGE(TAG, "Failed to save WS2812 state: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief 从NVS加载WS2812状态
 */
esp_err_t menu_nvs_manager_load_ws2812_state(MenuNvsManager_t* manager, bool* state) {
    if (!manager || !manager->common_manager || !state) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 获取基础管理器
    NvsCommonManager_t* common_wrapper = manager->common_manager;
    NvsBaseManager_t* base_manager = ((struct NvsCommonManagerWrapper*)common_wrapper)->base_manager;
    
    // 确保NVS命名空间已打开（使用只读模式）
    esp_err_t open_err = nvs_base_open(base_manager, true);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(open_err));
        
        // 打开失败时使用默认值
        *state = manager->default_ws2812_state;
        ESP_LOGW(TAG, "NVS namespace open failed, using default WS2812 state %d", *state);
        return ESP_OK;
    }
    
    // 加载数据
    esp_err_t err = nvs_common_manager_load_bool(manager->common_manager, 
                                               KEY_WS2812_STATE, 
                                               state);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded WS2812 state %d successfully", *state);
    } else if (err == ESP_ERR_NOT_FOUND) {
        // 如果找不到，使用默认值
        *state = manager->default_ws2812_state;
        ESP_LOGW(TAG, "WS2812 state not found, using default %d", *state);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to load WS2812 state: %s", esp_err_to_name(err));
        
        // 加载失败时也尝试使用默认值
        *state = manager->default_ws2812_state;
        ESP_LOGW(TAG, "Using default WS2812 state %d due to load failure", *state);
        // 仍然返回原始错误码，以便调用者知道加载失败但使用了默认值
    }
    
    return err;
}

/**
 * @brief 保存所有菜单配置到NVS
 */
esp_err_t menu_nvs_manager_save_all(MenuNvsManager_t* manager, uint8_t layer, bool ws2812_state, bool wifi_state) {
    if (!manager || !manager->common_manager) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 获取基础管理器
    NvsCommonManager_t* common_wrapper = manager->common_manager;
    NvsBaseManager_t* base_manager = common_wrapper->base_manager;
    
    // 确保NVS命名空间已打开（使用读写模式）
    esp_err_t open_err = nvs_base_open(base_manager, false);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(open_err));
        return open_err;
    }
    
    ESP_LOGI(TAG, "Preparing to save menu config: layer=%d, ws2812_state=%d, wifi_state=%d", layer, ws2812_state, wifi_state);
    
    // 保存层数据
    esp_err_t layer_err = nvs_common_manager_save_u32(manager->common_manager, 
                                                   KEY_CURRENT_LAYER, 
                                                   (uint32_t)layer);
    
    if (layer_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save current layer: %s", esp_err_to_name(layer_err));
        return layer_err;
    }
    
    // 保存WS2812状态
    esp_err_t ws_err = nvs_common_manager_save_bool(manager->common_manager, 
                                                  KEY_WS2812_STATE, 
                                                  ws2812_state);
    
    if (ws_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save WS2812 state: %s", esp_err_to_name(ws_err));
        return ws_err;
    }
    
    // 保存WiFi状态
    esp_err_t wifi_err = nvs_common_manager_save_bool(manager->common_manager, 
                                                    KEY_WIFI_STATE, 
                                                    wifi_state);
    
    if (wifi_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save WiFi state: %s", esp_err_to_name(wifi_err));
        return wifi_err;
    }
    
    // 统一提交所有更改以提高效率
    esp_err_t commit_err = nvs_base_commit(base_manager);
    if (commit_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit batch NVS changes: %s", 
                 esp_err_to_name(commit_err));
        return commit_err;
    }
    
    // 强制关闭并重新打开以确保数据真正写入
    nvs_base_close(base_manager);
    open_err = nvs_base_open(base_manager, true); // 只读模式重新打开
    if (open_err == ESP_OK) {
        // 验证数据是否正确保存
    uint32_t verify_layer = 0;
    bool verify_ws_state = false;
    bool verify_wifi_state = false;
    esp_err_t verify_layer_err = nvs_common_manager_load_u32(manager->common_manager, KEY_CURRENT_LAYER, &verify_layer);
    esp_err_t verify_ws_err = nvs_common_manager_load_bool(manager->common_manager, KEY_WS2812_STATE, &verify_ws_state);
    esp_err_t verify_wifi_err = nvs_common_manager_load_bool(manager->common_manager, KEY_WIFI_STATE, &verify_wifi_state);
    
    if (verify_layer_err == ESP_OK && verify_ws_err == ESP_OK && verify_wifi_err == ESP_OK && 
        verify_layer == (uint32_t)layer && verify_ws_state == ws2812_state && verify_wifi_state == wifi_state) {
            // 验证成功，不记录详细日志
        } else {
            ESP_LOGW(TAG, "Data verification after save: expected layer=%d, ws=%d, wifi=%d; got layer=%" PRIu32 " (err=%s), ws=%d (err=%s), wifi=%d (err=%s)",
                layer, ws2812_state, wifi_state, verify_layer, esp_err_to_name(verify_layer_err),
                verify_ws_state, esp_err_to_name(verify_ws_err),
                verify_wifi_state, esp_err_to_name(verify_wifi_err));
        }
        nvs_base_close(base_manager);
    }
    
    ESP_LOGI(TAG, "Batch saved and committed all menu configurations successfully");
    
    return ESP_OK;
}

/**
 * @brief 从NVS加载所有菜单配置
 */
esp_err_t menu_nvs_manager_load_all(MenuNvsManager_t* manager, uint8_t* layer, bool* ws2812_state, bool* wifi_state) {
    if (!manager || !manager->common_manager || !layer || !ws2812_state) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Attempting to load menu configurations");
    
    // 先加载层配置
    esp_err_t layer_err = menu_nvs_manager_load_current_layer(manager, layer);
    if (layer_err != ESP_OK && layer_err != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Error loading current layer, but continuing with other configurations");
    }
    
    // 再加载WS2812状态
    esp_err_t ws_err = menu_nvs_manager_load_ws2812_state(manager, ws2812_state);
    if (ws_err != ESP_OK && ws_err != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Error loading WS2812 state");
        return ws_err;
    }
    
    // 加载WiFi状态（如果提供了指针）
    esp_err_t wifi_err = ESP_OK;
    if (wifi_state) {
        wifi_err = menu_nvs_manager_load_wifi_state(manager, wifi_state);
        if (wifi_err != ESP_OK && wifi_err != ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Error loading WiFi state");
            return wifi_err;
        }
    }
    
    // 记录加载的配置值 - 已简化
    
    // 如果所有配置都找不到，返回ESP_OK但使用默认值
    if (layer_err == ESP_ERR_NOT_FOUND && ws_err == ESP_ERR_NOT_FOUND && 
        ((!wifi_state) || (wifi_state && wifi_err == ESP_ERR_NOT_FOUND))) {
        ESP_LOGW(TAG, "All menu configurations not found, using defaults");
    }
    
    // 只要至少有一个配置加载成功或使用了默认值，就返回成功
    return ESP_OK;
}

/**
 * @brief 重置所有菜单配置为默认值
 */
esp_err_t menu_nvs_manager_reset_to_default(MenuNvsManager_t* manager) {
    if (!manager || !manager->common_manager) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 保存默认值到NVS
    esp_err_t err = menu_nvs_manager_save_all(manager, 
                                            manager->default_layer, 
                                            manager->default_ws2812_state,
                                            manager->default_wifi_state);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Successfully reset all menu configurations to default");
    } else {
        ESP_LOGE(TAG, "Failed to reset all menu configurations to default: %s", 
                 esp_err_to_name(err));
    }
    
    return err;
}