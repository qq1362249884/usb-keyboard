#include "unified_nvs_manager.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// 内部结构定义
typedef struct {
    char namespace_name[32];      // 命名空间名称
    nvs_handle_t handle;          // NVS句柄
    bool initialized;             // 初始化标志
    bool opened;                  // 打开状态标志
    bool read_only;               // 只读模式标志
    bool auto_init;               // 自动初始化标志
    size_t max_blob_size;         // 最大二进制数据大小
} nvs_namespace_instance_t;

struct unified_nvs_manager_t {
    nvs_namespace_instance_t namespaces[NVS_NAMESPACE_COUNT]; // 命名空间实例数组
    bool global_initialized;      // 全局初始化标志
    nvs_error_callback_t error_callback; // 错误回调函数
    nvs_log_callback_t log_callback;    // 日志回调函数
};

// 默认命名空间配置
static const nvs_namespace_config_t default_namespace_configs[] = {
    [NVS_NAMESPACE_KEYMAP] = {"keymaps", true, false, 4096},     // 按键映射数据
    [NVS_NAMESPACE_MENU]   = {"menu", true, false, 1024},        // 菜单配置数据
    [NVS_NAMESPACE_WIFI]   = {"wifi", true, false, 512},        // WiFi配置数据
    [NVS_NAMESPACE_SYSTEM] = {"system", true, false, 256},       // 系统配置数据
    [NVS_NAMESPACE_CUSTOM] = {"custom", true, false, 2048},      // 自定义数据
};

// 内部日志函数
static void log_message(unified_nvs_manager_t* manager, const char* message, esp_log_level_t level) {
    if (manager && manager->log_callback) {
        manager->log_callback(message, level);
    } else {
        switch (level) {
            case ESP_LOG_ERROR:
                ESP_LOGE(UNIFIED_NVS_TAG, "%s", message);
                break;
            case ESP_LOG_WARN:
                ESP_LOGW(UNIFIED_NVS_TAG, "%s", message);
                break;
            case ESP_LOG_INFO:
                ESP_LOGI(UNIFIED_NVS_TAG, "%s", message);
                break;
            case ESP_LOG_DEBUG:
                ESP_LOGD(UNIFIED_NVS_TAG, "%s", message);
                break;
            case ESP_LOG_VERBOSE:
                ESP_LOGV(UNIFIED_NVS_TAG, "%s", message);
                break;
            default:
                ESP_LOGI(UNIFIED_NVS_TAG, "%s", message);
                break;
        }
    }
}

// 内部错误处理函数
static esp_err_t handle_error(unified_nvs_manager_t* manager, esp_err_t error, 
                             const char* namespace_name, const char* key) {
    if (error != ESP_OK && manager && manager->error_callback) {
        return manager->error_callback(error, namespace_name, key);
    }
    return error;
}

// 检查命名空间索引是否有效
static bool is_namespace_valid(nvs_namespace_t namespace) {
    return namespace >= 0 && namespace < NVS_NAMESPACE_COUNT;
}

// 初始化单个命名空间
static esp_err_t init_namespace(unified_nvs_manager_t* manager, nvs_namespace_t namespace) {
    if (!is_namespace_valid(namespace)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_namespace_instance_t* ns = &manager->namespaces[namespace];
    
    if (!ns->initialized) {
        // 打开命名空间
        esp_err_t err;
        if (ns->read_only) {
            err = nvs_open(ns->namespace_name, NVS_READONLY, &ns->handle);
        } else {
            err = nvs_open(ns->namespace_name, NVS_READWRITE, &ns->handle);
        }
        
        if (err != ESP_OK) {
            char message[128];
            snprintf(message, sizeof(message), "Failed to open namespace '%s': %s", 
                     ns->namespace_name, esp_err_to_name(err));
            log_message(manager, message, ESP_LOG_ERROR);
            return handle_error(manager, err, ns->namespace_name, NULL);
        }
        
        ns->opened = true;
        ns->initialized = true;
        
        char message[128];
        snprintf(message, sizeof(message), "Namespace '%s' initialized successfully", 
                 ns->namespace_name);
        log_message(manager, message, ESP_LOG_INFO);
    }
    
    return ESP_OK;
}

// 创建统一NVS管理器实例
unified_nvs_manager_t* unified_nvs_manager_create(const nvs_namespace_config_t* configs, size_t num_configs) {
    if (!configs || num_configs == 0) {
        ESP_LOGE(UNIFIED_NVS_TAG, "Invalid configuration parameters");
        return NULL;
    }
    
    unified_nvs_manager_t* manager = (unified_nvs_manager_t*)malloc(sizeof(unified_nvs_manager_t));
    if (!manager) {
        ESP_LOGE(UNIFIED_NVS_TAG, "Failed to allocate memory for unified NVS manager");
        return NULL;
    }
    
    // 初始化管理器结构
    memset(manager, 0, sizeof(unified_nvs_manager_t));
    manager->global_initialized = false;
    manager->error_callback = NULL;
    manager->log_callback = NULL;
    
    // 配置命名空间
    for (size_t i = 0; i < num_configs && i < NVS_NAMESPACE_COUNT; i++) {
        if (configs[i].namespace_name) {
            strncpy(manager->namespaces[i].namespace_name, configs[i].namespace_name, 
                   sizeof(manager->namespaces[i].namespace_name) - 1);
            manager->namespaces[i].namespace_name[sizeof(manager->namespaces[i].namespace_name) - 1] = '\0';
            
            manager->namespaces[i].read_only = configs[i].read_only;
            manager->namespaces[i].max_blob_size = configs[i].max_blob_size;
            manager->namespaces[i].auto_init = configs[i].auto_init;
        }
    }
    
    // 填充未配置的命名空间
    for (size_t i = num_configs; i < NVS_NAMESPACE_COUNT; i++) {
        if (i < sizeof(default_namespace_configs) / sizeof(default_namespace_configs[0])) {
            strncpy(manager->namespaces[i].namespace_name, default_namespace_configs[i].namespace_name, 
                   sizeof(manager->namespaces[i].namespace_name) - 1);
            manager->namespaces[i].namespace_name[sizeof(manager->namespaces[i].namespace_name) - 1] = '\0';
            
            manager->namespaces[i].read_only = default_namespace_configs[i].read_only;
            manager->namespaces[i].max_blob_size = default_namespace_configs[i].max_blob_size;
            manager->namespaces[i].auto_init = default_namespace_configs[i].auto_init;
        }
    }
    
    log_message(manager, "Unified NVS manager created successfully", ESP_LOG_INFO);
    return manager;
}

// 使用默认配置创建统一NVS管理器实例
unified_nvs_manager_t* unified_nvs_manager_create_default(void) {
    return unified_nvs_manager_create(default_namespace_configs, 
                                    sizeof(default_namespace_configs) / sizeof(default_namespace_configs[0]));
}

// 销毁统一NVS管理器实例
void unified_nvs_manager_destroy(unified_nvs_manager_t* manager) {
    if (!manager) {
        return;
    }
    
    // 关闭所有打开的命名空间
    for (int i = 0; i < NVS_NAMESPACE_COUNT; i++) {
        if (manager->namespaces[i].opened) {
            nvs_close(manager->namespaces[i].handle);
            manager->namespaces[i].opened = false;
            manager->namespaces[i].initialized = false;
        }
    }
    
    log_message(manager, "Unified NVS manager destroyed", ESP_LOG_INFO);
    free(manager);
}

// 初始化统一NVS管理器
esp_err_t unified_nvs_manager_init(unified_nvs_manager_t* manager) {
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (manager->global_initialized) {
        log_message(manager, "Unified NVS manager already initialized", ESP_LOG_WARN);
        return ESP_OK;
    }
    
    // 初始化NVS闪存
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        log_message(manager, "NVS partition needs to be erased. Erasing...", ESP_LOG_WARN);
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            char message[128];
            snprintf(message, sizeof(message), "Failed to erase NVS partition: %s", esp_err_to_name(err));
            log_message(manager, message, ESP_LOG_ERROR);
            return handle_error(manager, err, NULL, NULL);
        }
        // 再次初始化
        err = nvs_flash_init();
    }
    
    if (err != ESP_OK) {
        char message[128];
        snprintf(message, sizeof(message), "Failed to initialize NVS flash: %s", esp_err_to_name(err));
        log_message(manager, message, ESP_LOG_ERROR);
        return handle_error(manager, err, NULL, NULL);
    }
    
    // 初始化所有需要自动初始化的命名空间
    for (int i = 0; i < NVS_NAMESPACE_COUNT; i++) {
        if (manager->namespaces[i].auto_init) {
            err = init_namespace(manager, i);
            if (err != ESP_OK) {
                return err;
            }
        }
    }
    
    manager->global_initialized = true;
    log_message(manager, "Unified NVS manager initialized successfully", ESP_LOG_INFO);
    return ESP_OK;
}

// 设置错误回调函数
void unified_nvs_manager_set_error_callback(unified_nvs_manager_t* manager, nvs_error_callback_t callback) {
    if (manager) {
        manager->error_callback = callback;
    }
}

// 设置日志回调函数
void unified_nvs_manager_set_log_callback(unified_nvs_manager_t* manager, nvs_log_callback_t callback) {
    if (manager) {
        manager->log_callback = callback;
    }
}

// 保存数据到NVS
esp_err_t unified_nvs_manager_save(unified_nvs_manager_t* manager, 
                                   nvs_namespace_t namespace, 
                                   const char* key, 
                                   const void* data, 
                                   unified_nvs_data_type_t data_type, 
                                   size_t size) {
    if (!manager || !key || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_namespace_valid(namespace)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_namespace_instance_t* ns = &manager->namespaces[namespace];
    
    // 确保命名空间已初始化
    if (!ns->initialized) {
        esp_err_t err = init_namespace(manager, namespace);
        if (err != ESP_OK) {
            return err;
        }
    }
    
    if (ns->read_only) {
        char message[128];
        snprintf(message, sizeof(message), "Cannot save to read-only namespace '%s'", ns->namespace_name);
        log_message(manager, message, ESP_LOG_ERROR);
        return ESP_ERR_NVS_READ_ONLY;
    }
    
    esp_err_t err = ESP_OK;
    
    switch (data_type) {
        case UNIFIED_NVS_TYPE_U8:
            err = nvs_set_u8(ns->handle, key, *(const uint8_t*)data);
            break;
        case UNIFIED_NVS_TYPE_U16:
            err = nvs_set_u16(ns->handle, key, *(const uint16_t*)data);
            break;
        case UNIFIED_NVS_TYPE_U32:
            err = nvs_set_u32(ns->handle, key, *(const uint32_t*)data);
            break;
        case UNIFIED_NVS_TYPE_I8:
            err = nvs_set_i8(ns->handle, key, *(const int8_t*)data);
            break;
        case UNIFIED_NVS_TYPE_I16:
            err = nvs_set_i16(ns->handle, key, *(const int16_t*)data);
            break;
        case UNIFIED_NVS_TYPE_I32:
            err = nvs_set_i32(ns->handle, key, *(const int32_t*)data);
            break;
        case UNIFIED_NVS_TYPE_BOOL:
            err = nvs_set_u8(ns->handle, key, *(const bool*)data ? 1 : 0);
            break;
        case UNIFIED_NVS_TYPE_STR:
            err = nvs_set_str(ns->handle, key, (const char*)data);
            break;
        case UNIFIED_NVS_TYPE_BLOB:
            if (size > ns->max_blob_size) {
                char message[128];
                snprintf(message, sizeof(message), "Blob size %zu exceeds maximum %zu for namespace '%s'", 
                         size, ns->max_blob_size, ns->namespace_name);
                log_message(manager, message, ESP_LOG_ERROR);
                return ESP_ERR_NVS_INVALID_LENGTH;
            }
            err = nvs_set_blob(ns->handle, key, data, size);
            break;
        default:
            err = ESP_ERR_NOT_SUPPORTED;
            break;
    }
    
    if (err != ESP_OK) {
        char message[128];
        snprintf(message, sizeof(message), "Failed to save key '%s' in namespace '%s': %s", 
                 key, ns->namespace_name, esp_err_to_name(err));
        log_message(manager, message, ESP_LOG_ERROR);
    }
    
    return handle_error(manager, err, ns->namespace_name, key);
}

// 从NVS加载数据
esp_err_t unified_nvs_manager_load(unified_nvs_manager_t* manager, 
                                   nvs_namespace_t namespace, 
                                   const char* key, 
                                   void* data, 
                                   unified_nvs_data_type_t data_type, 
                                   size_t* size) {
    if (!manager || !key || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_namespace_valid(namespace)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_namespace_instance_t* ns = &manager->namespaces[namespace];
    
    // 确保命名空间已初始化
    if (!ns->initialized) {
        esp_err_t err = init_namespace(manager, namespace);
        if (err != ESP_OK) {
            return err;
        }
    }
    
    esp_err_t err = ESP_OK;
    
    switch (data_type) {
        case UNIFIED_NVS_TYPE_U8:
            err = nvs_get_u8(ns->handle, key, (uint8_t*)data);
            break;
        case UNIFIED_NVS_TYPE_U16:
            err = nvs_get_u16(ns->handle, key, (uint16_t*)data);
            break;
        case UNIFIED_NVS_TYPE_U32:
            err = nvs_get_u32(ns->handle, key, (uint32_t*)data);
            break;
        case UNIFIED_NVS_TYPE_I8:
            err = nvs_get_i8(ns->handle, key, (int8_t*)data);
            break;
        case UNIFIED_NVS_TYPE_I16:
            err = nvs_get_i16(ns->handle, key, (int16_t*)data);
            break;
        case UNIFIED_NVS_TYPE_I32:
            err = nvs_get_i32(ns->handle, key, (int32_t*)data);
            break;
        case UNIFIED_NVS_TYPE_BOOL: {
            uint8_t value;
            err = nvs_get_u8(ns->handle, key, &value);
            if (err == ESP_OK) {
                *(bool*)data = (value != 0);
            }
            break;
        }
        case UNIFIED_NVS_TYPE_STR: {
            if (size) {
                err = nvs_get_str(ns->handle, key, (char*)data, size);
            } else {
                // 先获取长度
                size_t len;
                err = nvs_get_str(ns->handle, key, NULL, &len);
                if (err == ESP_OK) {
                    err = nvs_get_str(ns->handle, key, (char*)data, &len);
                }
            }
            break;
        }
        case UNIFIED_NVS_TYPE_BLOB: {
            if (size) {
                err = nvs_get_blob(ns->handle, key, data, size);
            } else {
                // 先获取长度
                size_t len;
                err = nvs_get_blob(ns->handle, key, NULL, &len);
                if (err == ESP_OK) {
                    err = nvs_get_blob(ns->handle, key, data, &len);
                }
            }
            break;
        }
        default:
            err = ESP_ERR_NOT_SUPPORTED;
            break;
    }
    
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        char message[128];
        snprintf(message, sizeof(message), "Failed to load key '%s' from namespace '%s': %s", 
                 key, ns->namespace_name, esp_err_to_name(err));
        log_message(manager, message, ESP_LOG_ERROR);
    }
    
    return handle_error(manager, err, ns->namespace_name, key);
}

// 检查键是否存在
bool unified_nvs_manager_exists(unified_nvs_manager_t* manager, 
                                nvs_namespace_t namespace, 
                                const char* key) {
    if (!manager || !key) {
        return false;
    }
    
    if (!is_namespace_valid(namespace)) {
        return false;
    }
    
    nvs_namespace_instance_t* ns = &manager->namespaces[namespace];
    
    // 确保命名空间已初始化
    if (!ns->initialized) {
        esp_err_t err = init_namespace(manager, namespace);
        if (err != ESP_OK) {
            return false;
        }
    }
    
    // 尝试读取任意类型的数据来检查是否存在
    uint8_t dummy;
    esp_err_t err = nvs_get_u8(ns->handle, key, &dummy);
    return (err == ESP_OK);
}

// 删除指定键的数据
esp_err_t unified_nvs_manager_erase(unified_nvs_manager_t* manager, 
                                    nvs_namespace_t namespace, 
                                    const char* key) {
    if (!manager || !key) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_namespace_valid(namespace)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_namespace_instance_t* ns = &manager->namespaces[namespace];
    
    if (ns->read_only) {
        char message[128];
        snprintf(message, sizeof(message), "Cannot erase from read-only namespace '%s'", ns->namespace_name);
        log_message(manager, message, ESP_LOG_ERROR);
        return ESP_ERR_NVS_READ_ONLY;
    }
    
    // 确保命名空间已初始化
    if (!ns->initialized) {
        esp_err_t err = init_namespace(manager, namespace);
        if (err != ESP_OK) {
            return err;
        }
    }
    
    esp_err_t err = nvs_erase_key(ns->handle, key);
    
    if (err != ESP_OK) {
        char message[128];
        snprintf(message, sizeof(message), "Failed to erase key '%s' from namespace '%s': %s", 
                 key, ns->namespace_name, esp_err_to_name(err));
        log_message(manager, message, ESP_LOG_ERROR);
    }
    
    return handle_error(manager, err, ns->namespace_name, key);
}

// 提交所有更改到NVS
esp_err_t unified_nvs_manager_commit(unified_nvs_manager_t* manager) {
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t final_err = ESP_OK;
    
    for (int i = 0; i < NVS_NAMESPACE_COUNT; i++) {
        if (manager->namespaces[i].initialized && !manager->namespaces[i].read_only) {
            esp_err_t err = nvs_commit(manager->namespaces[i].handle);
            if (err != ESP_OK) {
                char message[128];
                snprintf(message, sizeof(message), "Failed to commit namespace '%s': %s", 
                         manager->namespaces[i].namespace_name, esp_err_to_name(err));
                log_message(manager, message, ESP_LOG_ERROR);
                final_err = err;
            }
        }
    }
    
    return final_err;
}

// 获取命名空间统计信息
esp_err_t unified_nvs_manager_get_stats(unified_nvs_manager_t* manager, 
                                        nvs_namespace_t namespace, 
                                        size_t* used_size, 
                                        size_t* free_size) {
    if (!manager || !used_size || !free_size) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_namespace_valid(namespace)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_namespace_instance_t* ns = &manager->namespaces[namespace];
    
    // 确保命名空间已初始化
    if (!ns->initialized) {
        esp_err_t err = init_namespace(manager, namespace);
        if (err != ESP_OK) {
            return err;
        }
    }
    
    // 获取统计信息
    nvs_stats_t nvs_stats;
    esp_err_t err = nvs_get_stats(ns->namespace_name, &nvs_stats);
    if (err != ESP_OK) {
        char message[128];
        snprintf(message, sizeof(message), "Failed to get stats for namespace '%s': %s", 
                 ns->namespace_name, esp_err_to_name(err));
        log_message(manager, message, ESP_LOG_ERROR);
        return handle_error(manager, err, ns->namespace_name, NULL);
    }
    
    *used_size = nvs_stats.used_entries;
    *free_size = nvs_stats.free_entries;
    
    return ESP_OK;
}

// 重置命名空间
esp_err_t unified_nvs_manager_reset_namespace(unified_nvs_manager_t* manager, 
                                              nvs_namespace_t namespace) {
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_namespace_valid(namespace)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_namespace_instance_t* ns = &manager->namespaces[namespace];
    
    if (ns->read_only) {
        char message[128];
        snprintf(message, sizeof(message), "Cannot reset read-only namespace '%s'", ns->namespace_name);
        log_message(manager, message, ESP_LOG_ERROR);
        return ESP_ERR_NVS_READ_ONLY;
    }
    
    // 确保命名空间已初始化
    if (!ns->initialized) {
        esp_err_t err = init_namespace(manager, namespace);
        if (err != ESP_OK) {
            return err;
        }
    }
    
    esp_err_t err = nvs_erase_all(ns->handle);
    if (err != ESP_OK) {
        char message[128];
        snprintf(message, sizeof(message), "Failed to reset namespace '%s': %s", 
                 ns->namespace_name, esp_err_to_name(err));
        log_message(manager, message, ESP_LOG_ERROR);
        return handle_error(manager, err, ns->namespace_name, NULL);
    }
    
    // 提交更改
    err = nvs_commit(ns->handle);
    if (err != ESP_OK) {
        char message[128];
        snprintf(message, sizeof(message), "Failed to commit after resetting namespace '%s': %s", 
                 ns->namespace_name, esp_err_to_name(err));
        log_message(manager, message, ESP_LOG_ERROR);
    }
    
    return handle_error(manager, err, ns->namespace_name, NULL);
}

// 按键映射专用API实现

// 保存按键映射数据
esp_err_t unified_nvs_save_keymap(unified_nvs_manager_t* manager, 
                                 uint8_t layer, 
                                 uint8_t key_index, 
                                 uint16_t key_code) {
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char key[32];
    snprintf(key, sizeof(key), "layer_%d_key_%d", layer, key_index);
    
    return UNIFIED_NVS_SAVE_U16(manager, NVS_NAMESPACE_KEYMAP, key, key_code);
}

// 加载按键映射数据
esp_err_t unified_nvs_load_keymap(unified_nvs_manager_t* manager, 
                                 uint8_t layer, 
                                 uint8_t key_index, 
                                 uint16_t* key_code) {
    if (!manager || !key_code) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char key[32];
    snprintf(key, sizeof(key), "layer_%d_key_%d", layer, key_index);
    
    return UNIFIED_NVS_LOAD_U16(manager, NVS_NAMESPACE_KEYMAP, key, key_code);
}

// 保存整个按键映射层
esp_err_t unified_nvs_save_keymap_layer(unified_nvs_manager_t* manager, 
                                      uint8_t layer, 
                                      const uint16_t* keymap, 
                                      uint8_t num_keys) {
    if (!manager || !keymap) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char key[32];
    snprintf(key, sizeof(key), "layer_%d", layer);
    
    return unified_nvs_manager_save(manager, NVS_NAMESPACE_KEYMAP, key, keymap, 
                                   UNIFIED_NVS_TYPE_BLOB, num_keys * sizeof(uint16_t));
}

// 加载整个按键映射层
esp_err_t unified_nvs_load_keymap_layer(unified_nvs_manager_t* manager, 
                                      uint8_t layer, 
                                      uint16_t* keymap, 
                                      uint8_t num_keys) {
    if (!manager || !keymap) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char key[32];
    snprintf(key, sizeof(key), "layer_%d", layer);
    
    size_t expected_size = num_keys * sizeof(uint16_t);
    return unified_nvs_manager_load(manager, NVS_NAMESPACE_KEYMAP, key, keymap, 
                                   UNIFIED_NVS_TYPE_BLOB, &expected_size);
}

// WiFi配置专用API实现

// 保存WiFi配置
esp_err_t unified_nvs_save_wifi_config(unified_nvs_manager_t* manager, 
                                      const char* ssid, 
                                      const char* password) {
    if (!manager || !ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t err;
    
    // 保存SSID
    err = UNIFIED_NVS_SAVE_STR(manager, NVS_NAMESPACE_WIFI, "ssid", ssid);
    if (err != ESP_OK) {
        return err;
    }
    
    // 保存密码
    err = UNIFIED_NVS_SAVE_STR(manager, NVS_NAMESPACE_WIFI, "password", password);
    
    return err;
}

// 加载WiFi配置
esp_err_t unified_nvs_load_wifi_config(unified_nvs_manager_t* manager, 
                                      char* ssid, 
                                      size_t ssid_len, 
                                      char* password, 
                                      size_t password_len) {
    if (!manager || !ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t err;
    
    // 加载SSID
    err = unified_nvs_manager_load(manager, NVS_NAMESPACE_WIFI, "ssid", ssid, 
                                   UNIFIED_NVS_TYPE_STR, &ssid_len);
    if (err != ESP_OK) {
        return err;
    }
    
    // 加载密码
    err = unified_nvs_manager_load(manager, NVS_NAMESPACE_WIFI, "password", password, 
                                   UNIFIED_NVS_TYPE_STR, &password_len);
    
    return err;
}

// 菜单配置专用API实现

// 保存菜单配置
esp_err_t unified_nvs_save_menu_config(unified_nvs_manager_t* manager, 
                                      uint8_t current_layer, 
                                      bool ws2812_state) {
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t err;
    
    // 保存当前层
    err = UNIFIED_NVS_SAVE_U8(manager, NVS_NAMESPACE_MENU, "current_layer", current_layer);
    if (err != ESP_OK) {
        return err;
    }
    
    // 保存WS2812状态
    err = UNIFIED_NVS_SAVE_BOOL(manager, NVS_NAMESPACE_MENU, "ws2812_state", ws2812_state);
    
    return err;
}

// 加载菜单配置
esp_err_t unified_nvs_load_menu_config(unified_nvs_manager_t* manager, 
                                      uint8_t* current_layer, 
                                      bool* ws2812_state) {
    if (!manager || !current_layer || !ws2812_state) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t err;
    
    // 加载当前层
    err = UNIFIED_NVS_LOAD_U8(manager, NVS_NAMESPACE_MENU, "current_layer", current_layer);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        return err;
    }
    
    // 加载WS2812状态
    err = UNIFIED_NVS_LOAD_BOOL(manager, NVS_NAMESPACE_MENU, "ws2812_state", ws2812_state);
    
    return (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) ? ESP_OK : err;
}

// WiFi状态和模式配置专用API实现

// 保存WiFi模式配置
esp_err_t unified_nvs_save_wifi_state_config(unified_nvs_manager_t* manager, 
                                            uint8_t wifi_mode) {
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 保存WiFi模式
    return UNIFIED_NVS_SAVE_U8(manager, NVS_NAMESPACE_WIFI, "wifi_mode", wifi_mode);
}

// 加载WiFi模式配置
esp_err_t unified_nvs_load_wifi_state_config(unified_nvs_manager_t* manager, 
                                            uint8_t* wifi_mode) {
    if (!manager || !wifi_mode) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 加载WiFi模式
    return UNIFIED_NVS_LOAD_U8(manager, NVS_NAMESPACE_WIFI, "wifi_mode", wifi_mode);
}

// 清除WiFi密码配置
esp_err_t unified_nvs_clear_wifi_password(unified_nvs_manager_t* manager) {
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t err;
    
    // 清除WiFi密码
    err = unified_nvs_manager_erase(manager, NVS_NAMESPACE_WIFI, "password");
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        return err;
    }
    
    // 清除WiFiSSID
    err = unified_nvs_manager_erase(manager, NVS_NAMESPACE_WIFI, "ssid");
    
    return (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) ? ESP_OK : err;
}