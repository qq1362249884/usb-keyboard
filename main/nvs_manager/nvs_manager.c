#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

static const char* TAG_BASE = "NVS_BASE";
static const char* TAG_COMMON = "NVS_COMMON";

/**
 * @brief NVS基础管理器结构定义
 */
struct NvsBaseManager {
    char namespace_name[32];      // NVS命名空间名称
    nvs_handle_t handle;          // NVS句柄
    bool initialized;             // 初始化标志
    bool opened;                  // 打开状态标志
};

/**
 * @brief NVS基础管理器句柄
 */
typedef struct NvsBaseManager NvsBaseManager_t;

/**
 * @brief 通用NVS管理器结构定义
 */
struct NvsCommonManagerWrapper {
    NvsBaseManager_t* base_manager;
};

/**
 * @brief 通用NVS管理器接口
 * 
 * 这个接口提供了通用的NVS操作功能，可以用于管理各种类型的数据存储
 */
typedef struct NvsCommonManagerWrapper NvsCommonManager_t;

/**
 * @brief 创建NVS基础管理器实例
 * @param namespace_name NVS命名空间名称
 * @return 管理器实例句柄，失败返回NULL
 */
NvsBaseManager_t* nvs_base_create(const char* namespace_name) {
    if (!namespace_name) {
        ESP_LOGE(TAG_BASE, "Namespace name cannot be NULL");
        return NULL;
    }

    // 检查命名空间名称长度
    size_t name_len = strlen(namespace_name);
    if (name_len >= sizeof(((NvsBaseManager_t*)0)->namespace_name)) {
        ESP_LOGE(TAG_BASE, "Namespace name too long (max %zu characters)", 
                sizeof(((NvsBaseManager_t*)0)->namespace_name) - 1);
        return NULL;
    }

    NvsBaseManager_t* manager = (NvsBaseManager_t*)malloc(sizeof(NvsBaseManager_t));
    if (!manager) {
        ESP_LOGE(TAG_BASE, "Failed to allocate memory for NVS base manager");
        return NULL;
    }

    // 初始化管理器
    strcpy(manager->namespace_name, namespace_name);
    manager->handle = 0;
    manager->initialized = false;
    manager->opened = false;

    return manager;
}

/**
 * @brief 销毁NVS基础管理器实例
 * @param manager 管理器实例句柄
 */
void nvs_base_destroy(NvsBaseManager_t* manager) {
    if (!manager) {
        return;
    }

    // 确保关闭NVS
    if (manager->opened) {
        nvs_close(manager->handle);
    }

    free(manager);
}

/**
 * @brief 初始化NVS基础管理器
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_init(NvsBaseManager_t* manager) {
    if (!manager) {
        return ESP_FAIL;
    }

    // 初始化NVS闪存
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS分区被截断，需要擦除
        ESP_LOGW(TAG_BASE, "NVS partition needs to be erased. Erasing...");
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            ESP_LOGE(TAG_BASE, "Failed to erase NVS partition: %s", esp_err_to_name(err));
            return err;
        }
        // 再次初始化
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return err;
    }

    manager->initialized = true;
    return ESP_OK;
}

/**
 * @brief 打开NVS命名空间
 * @param manager 管理器实例句柄
 * @param read_only 是否以只读模式打开
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_open(NvsBaseManager_t* manager, bool read_only) {
    if (!manager || !manager->initialized) {
        return ESP_FAIL;
    }

    // 已经打开则先关闭
    if (manager->opened) {
        nvs_close(manager->handle);
        manager->opened = false;
    }

    // 打开NVS命名空间
    esp_err_t err;
    if (read_only) {
        err = nvs_open(manager->namespace_name, NVS_READONLY, &manager->handle);
    } else {
        err = nvs_open(manager->namespace_name, NVS_READWRITE, &manager->handle);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to open NVS namespace '%s': %s", 
                manager->namespace_name, esp_err_to_name(err));
        return err;
    }

    manager->opened = true;
    return ESP_OK;
}

/**
 * @brief 关闭NVS命名空间
 * @param manager 管理器实例句柄
 */
void nvs_base_close(NvsBaseManager_t* manager) {
    if (!manager || !manager->opened) {
        return;
    }

    nvs_close(manager->handle);
    manager->opened = false;
}

/**
 * @brief 提交所有更改到NVS
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_commit(NvsBaseManager_t* manager) {
    if (!manager || !manager->opened) {
        return ESP_FAIL;
    }

    esp_err_t err = nvs_commit(manager->handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to commit NVS changes: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/**
 * @brief 保存二进制数据到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param data 数据指针
 * @param size 数据大小
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_save_blob(NvsBaseManager_t* manager, const char* key, const void* data, size_t size) {
    if (!manager || !key || !data || !manager->opened) {
        return ESP_FAIL;
    }

    esp_err_t err = nvs_set_blob(manager->handle, key, data, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to save blob '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    // 提交更改
    return nvs_base_commit(manager);
}

/**
 * @brief 从NVS加载二进制数据
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param data 数据缓冲区
 * @param size 输入为缓冲区大小，输出为实际读取大小
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_load_blob(NvsBaseManager_t* manager, const char* key, void* data, size_t* size) {
    if (!manager || !key || !data || !size || !manager->opened) {
        return ESP_FAIL;
    }

    esp_err_t err = nvs_get_blob(manager->handle, key, NULL, size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG_BASE, "Blob '%s' not found", key);
        return err;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to get blob size '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    // 再次调用获取实际数据
    err = nvs_get_blob(manager->handle, key, data, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to load blob '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/**
 * @brief 保存字符串到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param str 字符串
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_save_str(NvsBaseManager_t* manager, const char* key, const char* str) {
    if (!manager || !key || !str || !manager->opened) {
        return ESP_FAIL;
    }

    esp_err_t err = nvs_set_str(manager->handle, key, str);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to save string '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    // 提交更改
    return nvs_base_commit(manager);
}

/**
 * @brief 从NVS加载字符串
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param str 字符串缓冲区
 * @param str_len 缓冲区长度
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_load_str(NvsBaseManager_t* manager, const char* key, char* str, size_t str_len) {
    if (!manager || !key || !str || str_len == 0 || !manager->opened) {
        return ESP_FAIL;
    }

    esp_err_t err = nvs_get_str(manager->handle, key, str, &str_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG_BASE, "String '%s' not found", key);
        str[0] = '\0'; // 确保字符串为空
        return err;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to load string '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/**
 * @brief 保存无符号整数到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 整数值
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_save_u32(NvsBaseManager_t* manager, const char* key, uint32_t value) {
    if (!manager || !key || !manager->opened) {
        return ESP_FAIL;
    }

    esp_err_t err = nvs_set_u32(manager->handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to save u32 '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    // 提交更改
    return nvs_base_commit(manager);
}

/**
 * @brief 从NVS加载无符号整数
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 整数值缓冲区
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_load_u32(NvsBaseManager_t* manager, const char* key, uint32_t* value) {
    if (!manager || !key || !value || !manager->opened) {
        return ESP_FAIL;
    }

    esp_err_t err = nvs_get_u32(manager->handle, key, value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG_BASE, "u32 '%s' not found", key);
        return err;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to load u32 '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/**
 * @brief 保存布尔值到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 布尔值
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_save_bool(NvsBaseManager_t* manager, const char* key, bool value) {
    if (!manager || !key || !manager->opened) {
        return ESP_FAIL;
    }

    // 将布尔值转换为整数保存
    esp_err_t err = nvs_set_u8(manager->handle, key, (uint8_t)value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to save bool '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    // 提交更改
    return nvs_base_commit(manager);
}

/**
 * @brief 从NVS加载布尔值
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 布尔值缓冲区
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_load_bool(NvsBaseManager_t* manager, const char* key, bool* value) {
    if (!manager || !key || !value || !manager->opened) {
        return ESP_FAIL;
    }

    uint8_t bool_value;
    esp_err_t err = nvs_get_u8(manager->handle, key, &bool_value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG_BASE, "bool '%s' not found", key);
        return err;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to load bool '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    *value = (bool_value != 0);
    return ESP_OK;
}

/**
 * @brief 从NVS删除指定键的数据
 * @param manager 管理器实例句柄
 * @param key 键名
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_erase(NvsBaseManager_t* manager, const char* key) {
    if (!manager || !key || !manager->opened) {
        return ESP_FAIL;
    }

    esp_err_t err = nvs_erase_key(manager->handle, key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_BASE, "Failed to erase key '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    // 提交更改
    return nvs_base_commit(manager);
}

/**
 * @brief 检查键是否存在于NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @return true 存在，false 不存在
 */
bool nvs_base_exists(NvsBaseManager_t* manager, const char* key) {
    if (!manager || !key || !manager->opened) {
        return false;
    }

    // 尝试读取键值大小来检查是否存在，这种方法对所有类型有效
    size_t required_size = 0;
    esp_err_t err = nvs_get_blob(manager->handle, key, NULL, &required_size);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // 尝试以字符串类型检查
        char dummy[1];
        size_t dummy_size = sizeof(dummy);
        err = nvs_get_str(manager->handle, key, dummy, &dummy_size);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            return false;
        }
    }

    // 如果返回的是ESP_OK或者ESP_ERR_NVS_INVALID_LENGTH（表示存在但需要更大的缓冲区），都表示键存在
    return (err == ESP_OK || err == ESP_ERR_NVS_INVALID_LENGTH);
}

/**
 * @brief 获取错误信息字符串
 * @param err 错误码
 * @return 错误信息字符串
 */
const char* nvs_base_get_error_string(esp_err_t err) {
    // 使用ESP-IDF内置的错误字符串函数
    return esp_err_to_name(err);
}

/**
 * @brief 创建通用NVS管理器实例
 * @param namespace_name NVS命名空间名称
 * @return 管理器实例句柄
 */
NvsCommonManager_t* nvs_common_manager_create(const char* namespace_name) {
    if (!namespace_name) {
        ESP_LOGE(TAG_COMMON, "Namespace name cannot be NULL");
        return NULL;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)malloc(sizeof(struct NvsCommonManagerWrapper));
    if (!wrapper) {
        ESP_LOGE(TAG_COMMON, "Failed to allocate memory for common manager wrapper");
        return NULL;
    }

    // 创建基础管理器
    wrapper->base_manager = nvs_base_create(namespace_name);
    if (!wrapper->base_manager) {
        ESP_LOGE(TAG_COMMON, "Failed to create base manager");
        free(wrapper);
        return NULL;
    }

    return wrapper;
}

/**
 * @brief 销毁通用NVS管理器实例
 * @param manager 管理器实例句柄
 */
void nvs_common_manager_destroy(NvsCommonManager_t* manager) {
    if (!manager) {
        return;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    nvs_base_destroy(wrapper->base_manager);
    free(wrapper);
}

/**
 * @brief 初始化通用NVS管理器
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_init(NvsCommonManager_t* manager) {
    esp_err_t result = ESP_FAIL;
    if (!manager) {
        return ESP_FAIL;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    result = nvs_base_init(wrapper->base_manager);
    return result;
}

/**
 * @brief 保存二进制数据到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param data 数据指针
 * @param size 数据大小
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_save_blob(NvsCommonManager_t* manager, 
                                      const char* key, 
                                      const void* data, 
                                      size_t size) {
    esp_err_t result = ESP_FAIL;
    if (!manager || !key || !data) {
        return ESP_FAIL;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    result = nvs_base_save_blob(wrapper->base_manager, key, data, size);
    return result;
}

/**
 * @brief 从NVS加载二进制数据
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param data 数据缓冲区
 * @param size 数据大小
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_load_blob(NvsCommonManager_t* manager, 
                                      const char* key, 
                                      void* data, 
                                      size_t* size) {
    esp_err_t result = ESP_FAIL;
    if (!manager || !key || !data || !size) {
        return ESP_FAIL;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    result = nvs_base_load_blob(wrapper->base_manager, key, data, size);
    return result;
}

/**
 * @brief 保存字符串到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param str 字符串
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_save_str(NvsCommonManager_t* manager, 
                                     const char* key, 
                                     const char* str) {
    esp_err_t result = ESP_FAIL;
    if (!manager || !key || !str) {
        return ESP_FAIL;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    result = nvs_base_save_str(wrapper->base_manager, key, str);
    return result;
}

/**
 * @brief 从NVS加载字符串
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param str 字符串缓冲区
 * @param str_len 字符串缓冲区长度
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_load_str(NvsCommonManager_t* manager, 
                                     const char* key, 
                                     char* str, 
                                     size_t str_len) {
    esp_err_t result = ESP_FAIL;
    if (!manager || !key || !str || str_len == 0) {
        return ESP_FAIL;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    result = nvs_base_load_str(wrapper->base_manager, key, str, str_len);
    return result;
}

/**
 * @brief 保存整数到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 整数值
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_save_u32(NvsCommonManager_t* manager, 
                                     const char* key, 
                                     uint32_t value) {
    esp_err_t result = ESP_FAIL;
    if (!manager || !key) {
        return ESP_FAIL;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    result = nvs_base_save_u32(wrapper->base_manager, key, value);
    return result;
}

/**
 * @brief 从NVS加载整数
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 整数值缓冲区
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_load_u32(NvsCommonManager_t* manager, 
                                     const char* key, 
                                     uint32_t* value) {
    esp_err_t result = ESP_FAIL;
    if (!manager || !key || !value) {
        return ESP_FAIL;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    result = nvs_base_load_u32(wrapper->base_manager, key, value);
    return result;
}

/**
 * @brief 保存布尔值到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 布尔值
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_save_bool(NvsCommonManager_t* manager, 
                                      const char* key, 
                                      bool value) {
    esp_err_t result = ESP_FAIL;
    if (!manager || !key) {
        return ESP_FAIL;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    result = nvs_base_save_bool(wrapper->base_manager, key, value);
    return result;
}

/**
 * @brief 从NVS加载布尔值
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 布尔值缓冲区
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_load_bool(NvsCommonManager_t* manager, 
                                      const char* key, 
                                      bool* value) {
    esp_err_t result = ESP_FAIL;
    if (!manager || !key || !value) {
        return ESP_FAIL;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    result = nvs_base_load_bool(wrapper->base_manager, key, value);
    return result;
}

/**
 * @brief 从NVS删除数据
 * @param manager 管理器实例句柄
 * @param key 键名
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_erase(NvsCommonManager_t* manager, const char* key) {
    esp_err_t result = ESP_FAIL;
    if (!manager || !key) {
        return ESP_FAIL;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    result = nvs_base_erase(wrapper->base_manager, key);
    return result;
}

/**
 * @brief 检查键是否存在
 * @param manager 管理器实例句柄
 * @param key 键名
 * @return 1 存在，0 不存在
 */
int nvs_common_manager_exists(NvsCommonManager_t* manager, const char* key) {
    int result = 0;
    if (!manager || !key) {
        return 0;
    }

    struct NvsCommonManagerWrapper* wrapper = (struct NvsCommonManagerWrapper*)manager;
    bool exists = nvs_base_exists(wrapper->base_manager, key);
    result = exists ? 1 : 0;
    return result;
}

/**
 * @brief 获取错误信息字符串
 * @param manager 管理器实例句柄
 * @param err 错误码
 * @return 错误信息字符串
 */
const char* nvs_common_manager_get_error_string(NvsCommonManager_t* manager, esp_err_t err) {
    if (!manager) {
        return "Unknown error (manager NULL)";
    }

    return nvs_base_get_error_string(err);
}

#ifdef __cplusplus
}
#endif