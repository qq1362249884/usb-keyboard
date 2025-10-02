#ifndef UNIFIED_NVS_MANAGER_H
#define UNIFIED_NVS_MANAGER_H

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// 日志标签
#define UNIFIED_NVS_TAG "UNIFIED_NVS"

// NVS命名空间定义
typedef enum {
    NVS_NAMESPACE_KEYMAP = 0,     // 按键映射数据
    NVS_NAMESPACE_MENU,          // 菜单配置数据
    NVS_NAMESPACE_WIFI,          // WiFi配置数据
    NVS_NAMESPACE_SYSTEM,        // 系统配置数据
    NVS_NAMESPACE_CUSTOM,        // 自定义数据
    NVS_NAMESPACE_COUNT          // 命名空间数量
} nvs_namespace_t;

// NVS数据类型枚举
typedef enum {
    UNIFIED_NVS_TYPE_U8 = 0,             // uint8_t
    UNIFIED_NVS_TYPE_U16,                // uint16_t
    UNIFIED_NVS_TYPE_U32,                // uint32_t
    UNIFIED_NVS_TYPE_I8,                 // int8_t
    UNIFIED_NVS_TYPE_I16,                // int16_t
    UNIFIED_NVS_TYPE_I32,                // int32_t
    UNIFIED_NVS_TYPE_BOOL,               // bool
    UNIFIED_NVS_TYPE_STR,                // 字符串
    UNIFIED_NVS_TYPE_BLOB,               // 二进制数据
    UNIFIED_NVS_TYPE_COUNT               // 数据类型数量
} unified_nvs_data_type_t;

// 统一NVS管理器配置结构
typedef struct {
    const char* namespace_name;  // 命名空间名称
    bool auto_init;              // 是否自动初始化
    bool read_only;              // 是否只读模式
    size_t max_blob_size;        // 最大二进制数据大小
} nvs_namespace_config_t;

// 统一NVS管理器句柄
typedef struct unified_nvs_manager_t unified_nvs_manager_t;

// 回调函数类型定义
typedef esp_err_t (*nvs_error_callback_t)(esp_err_t error, const char* namespace_name, const char* key);
typedef void (*nvs_log_callback_t)(const char* message, esp_log_level_t level);

/**
 * @brief 创建统一NVS管理器实例
 * @param configs 命名空间配置数组
 * @param num_configs 配置数量
 * @return 管理器实例句柄，失败返回NULL
 */
unified_nvs_manager_t* unified_nvs_manager_create(const nvs_namespace_config_t* configs, size_t num_configs);

/**
 * @brief 使用默认配置创建统一NVS管理器实例
 * @return 管理器实例句柄，失败返回NULL
 */
unified_nvs_manager_t* unified_nvs_manager_create_default(void);

/**
 * @brief 销毁统一NVS管理器实例
 * @param manager 管理器实例句柄
 */
void unified_nvs_manager_destroy(unified_nvs_manager_t* manager);

/**
 * @brief 初始化统一NVS管理器
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_manager_init(unified_nvs_manager_t* manager);

/**
 * @brief 设置错误回调函数
 * @param manager 管理器实例句柄
 * @param callback 错误回调函数
 */
void unified_nvs_manager_set_error_callback(unified_nvs_manager_t* manager, nvs_error_callback_t callback);

/**
 * @brief 设置日志回调函数
 * @param manager 管理器实例句柄
 * @param callback 日志回调函数
 */
void unified_nvs_manager_set_log_callback(unified_nvs_manager_t* manager, nvs_log_callback_t callback);

/**
 * @brief 保存数据到NVS
 * @param manager 管理器实例句柄
 * @param namespace 命名空间
 * @param key 键名
 * @param data 数据指针
 * @param data_type 数据类型
 * @param size 数据大小（对于字符串和二进制数据）
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_manager_save(unified_nvs_manager_t* manager, 
                                   nvs_namespace_t namespace, 
                                   const char* key, 
                                   const void* data, 
                                   unified_nvs_data_type_t data_type, 
                                   size_t size);

/**
 * @brief 从NVS加载数据
 * @param manager 管理器实例句柄
 * @param namespace 命名空间
 * @param key 键名
 * @param data 数据缓冲区
 * @param data_type 数据类型
 * @param size 输入为缓冲区大小，输出为实际读取大小
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_manager_load(unified_nvs_manager_t* manager, 
                                   nvs_namespace_t namespace, 
                                   const char* key, 
                                   void* data, 
                                   unified_nvs_data_type_t data_type, 
                                   size_t* size);

/**
 * @brief 检查键是否存在
 * @param manager 管理器实例句柄
 * @param namespace 命名空间
 * @param key 键名
 * @return true 存在，false 不存在
 */
bool unified_nvs_manager_exists(unified_nvs_manager_t* manager, 
                                nvs_namespace_t namespace, 
                                const char* key);

/**
 * @brief 删除指定键的数据
 * @param manager 管理器实例句柄
 * @param namespace 命名空间
 * @param key 键名
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_manager_erase(unified_nvs_manager_t* manager, 
                                    nvs_namespace_t namespace, 
                                    const char* key);

/**
 * @brief 提交所有更改到NVS
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_manager_commit(unified_nvs_manager_t* manager);

/**
 * @brief 获取命名空间统计信息
 * @param manager 管理器实例句柄
 * @param namespace 命名空间
 * @param used_size 已使用大小（输出）
 * @param free_size 剩余大小（输出）
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_manager_get_stats(unified_nvs_manager_t* manager, 
                                        nvs_namespace_t namespace, 
                                        size_t* used_size, 
                                        size_t* free_size);

/**
 * @brief 重置命名空间（删除所有数据）
 * @param manager 管理器实例句柄
 * @param namespace 命名空间
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_manager_reset_namespace(unified_nvs_manager_t* manager, 
                                              nvs_namespace_t namespace);

// 便捷宏定义
#define UNIFIED_NVS_SAVE_U8(manager, ns, key, value) \
    unified_nvs_manager_save(manager, ns, key, &(value), UNIFIED_NVS_TYPE_U8, sizeof(uint8_t))

#define UNIFIED_NVS_SAVE_U16(manager, ns, key, value) \
    unified_nvs_manager_save(manager, ns, key, &(value), UNIFIED_NVS_TYPE_U16, sizeof(uint16_t))

#define UNIFIED_NVS_SAVE_U32(manager, ns, key, value) \
    unified_nvs_manager_save(manager, ns, key, &(value), UNIFIED_NVS_TYPE_U32, sizeof(uint32_t))

#define UNIFIED_NVS_SAVE_BOOL(manager, ns, key, value) \
    unified_nvs_manager_save(manager, ns, key, &(value), UNIFIED_NVS_TYPE_BOOL, sizeof(bool))

#define UNIFIED_NVS_SAVE_STR(manager, ns, key, str) \
    unified_nvs_manager_save(manager, ns, key, str, UNIFIED_NVS_TYPE_STR, strlen(str) + 1)

#define UNIFIED_NVS_LOAD_U8(manager, ns, key, value) \
    unified_nvs_manager_load(manager, ns, key, value, UNIFIED_NVS_TYPE_U8, NULL)

#define UNIFIED_NVS_LOAD_U16(manager, ns, key, value) \
    unified_nvs_manager_load(manager, ns, key, value, UNIFIED_NVS_TYPE_U16, NULL)

#define UNIFIED_NVS_LOAD_U32(manager, ns, key, value) \
    unified_nvs_manager_load(manager, ns, key, value, UNIFIED_NVS_TYPE_U32, NULL)

#define UNIFIED_NVS_LOAD_BOOL(manager, ns, key, value) \
    unified_nvs_manager_load(manager, ns, key, value, UNIFIED_NVS_TYPE_BOOL, NULL)

// 按键映射专用API
/**
 * @brief 保存按键映射数据
 * @param manager 管理器实例句柄
 * @param layer 层索引
 * @param key_index 按键索引
 * @param key_code 键码
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_save_keymap(unified_nvs_manager_t* manager, 
                                 uint8_t layer, 
                                 uint8_t key_index, 
                                 uint16_t key_code);

/**
 * @brief 加载按键映射数据
 * @param manager 管理器实例句柄
 * @param layer 层索引
 * @param key_index 按键索引
 * @param key_code 键码缓冲区
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_load_keymap(unified_nvs_manager_t* manager, 
                                 uint8_t layer, 
                                 uint8_t key_index, 
                                 uint16_t* key_code);

/**
 * @brief 保存整个按键映射层
 * @param manager 管理器实例句柄
 * @param layer 层索引
 * @param keymap 按键映射数组
 * @param num_keys 按键数量
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_save_keymap_layer(unified_nvs_manager_t* manager, 
                                      uint8_t layer, 
                                      const uint16_t* keymap, 
                                      uint8_t num_keys);

/**
 * @brief 加载整个按键映射层
 * @param manager 管理器实例句柄
 * @param layer 层索引
 * @param keymap 按键映射数组
 * @param num_keys 按键数量
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_load_keymap_layer(unified_nvs_manager_t* manager, 
                                      uint8_t layer, 
                                      uint16_t* keymap, 
                                      uint8_t num_keys);

// WiFi配置专用API
/**
 * @brief 保存WiFi配置
 * @param manager 管理器实例句柄
 * @param ssid SSID
 * @param password 密码
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_save_wifi_config(unified_nvs_manager_t* manager, 
                                      const char* ssid, 
                                      const char* password);

/**
 * @brief 加载WiFi配置
 * @param manager 管理器实例句柄
 * @param ssid SSID缓冲区
 * @param ssid_len SSID缓冲区长度
 * @param password 密码缓冲区
 * @param password_len 密码缓冲区长度
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_load_wifi_config(unified_nvs_manager_t* manager, 
                                      char* ssid, 
                                      size_t ssid_len, 
                                      char* password, 
                                      size_t password_len);

// 菜单配置专用API
/**
 * @brief 保存菜单配置
 * @param manager 管理器实例句柄
 * @param current_layer 当前层
 * @param ws2812_state WS2812状态
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_save_menu_config(unified_nvs_manager_t* manager, 
                                      uint8_t current_layer, 
                                      bool ws2812_state);

/**
 * @brief 加载菜单配置
 * @param manager 管理器实例句柄
 * @param current_layer 当前层缓冲区
 * @param ws2812_state WS2812状态缓冲区
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_load_menu_config(unified_nvs_manager_t* manager, 
                                      uint8_t* current_layer, 
                                      bool* ws2812_state);

// WiFi状态和模式配置专用API
/**
 * @brief 保存WiFi模式配置
 * @param manager 管理器实例句柄
 * @param wifi_mode WiFi模式
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_save_wifi_state_config(unified_nvs_manager_t* manager, 
                                            uint8_t wifi_mode);

/**
 * @brief 加载WiFi模式配置
 * @param manager 管理器实例句柄
 * @param wifi_mode WiFi模式缓冲区
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_load_wifi_state_config(unified_nvs_manager_t* manager, 
                                            uint8_t* wifi_mode);

/**
 * @brief 清除WiFi密码配置
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t unified_nvs_clear_wifi_password(unified_nvs_manager_t* manager);

#ifdef __cplusplus
}
#endif

#endif // UNIFIED_NVS_MANAGER_H