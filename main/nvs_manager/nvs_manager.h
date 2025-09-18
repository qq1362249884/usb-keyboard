#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

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

// 基础管理器API函数声明

/**
 * @brief 创建NVS基础管理器实例
 * @param namespace_name NVS命名空间名称
 * @return 管理器实例句柄，失败返回NULL
 */
NvsBaseManager_t* nvs_base_create(const char* namespace_name);

/**
 * @brief 销毁NVS基础管理器实例
 * @param manager 管理器实例句柄
 */
void nvs_base_destroy(NvsBaseManager_t* manager);

/**
 * @brief 初始化NVS基础管理器
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_init(NvsBaseManager_t* manager);

/**
 * @brief 打开NVS命名空间
 * @param manager 管理器实例句柄
 * @param read_only 是否以只读模式打开
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_open(NvsBaseManager_t* manager, bool read_only);

/**
 * @brief 关闭NVS命名空间
 * @param manager 管理器实例句柄
 */
void nvs_base_close(NvsBaseManager_t* manager);

/**
 * @brief 提交所有更改到NVS
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_commit(NvsBaseManager_t* manager);

/**
 * @brief 保存二进制数据到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param data 数据指针
 * @param size 数据大小
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_save_blob(NvsBaseManager_t* manager, const char* key, const void* data, size_t size);

/**
 * @brief 从NVS加载二进制数据
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param data 数据缓冲区
 * @param size 输入为缓冲区大小，输出为实际读取大小
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_load_blob(NvsBaseManager_t* manager, const char* key, void* data, size_t* size);

/**
 * @brief 保存字符串到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param str 字符串
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_save_str(NvsBaseManager_t* manager, const char* key, const char* str);

/**
 * @brief 从NVS加载字符串
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param str 字符串缓冲区
 * @param str_len 缓冲区长度
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_load_str(NvsBaseManager_t* manager, const char* key, char* str, size_t str_len);

/**
 * @brief 保存无符号整数到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 整数值
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_save_u32(NvsBaseManager_t* manager, const char* key, uint32_t value);

/**
 * @brief 从NVS加载无符号整数
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 整数值缓冲区
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_load_u32(NvsBaseManager_t* manager, const char* key, uint32_t* value);

/**
 * @brief 保存布尔值到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 布尔值
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_save_bool(NvsBaseManager_t* manager, const char* key, bool value);

/**
 * @brief 从NVS加载布尔值
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 布尔值缓冲区
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_load_bool(NvsBaseManager_t* manager, const char* key, bool* value);

/**
 * @brief 从NVS删除指定键的数据
 * @param manager 管理器实例句柄
 * @param key 键名
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_base_erase(NvsBaseManager_t* manager, const char* key);

/**
 * @brief 检查键是否存在于NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @return true 存在，false 不存在
 */
bool nvs_base_exists(NvsBaseManager_t* manager, const char* key);

/**
 * @brief 获取错误信息字符串
 * @param err 错误码
 * @return 错误信息字符串
 */
const char* nvs_base_get_error_string(esp_err_t err);

// 通用管理器API函数声明

/**
 * @brief 创建通用NVS管理器实例
 * @param namespace_name NVS命名空间名称
 * @return 管理器实例句柄
 */
NvsCommonManager_t* nvs_common_manager_create(const char* namespace_name);

/**
 * @brief 销毁通用NVS管理器实例
 * @param manager 管理器实例句柄
 */
void nvs_common_manager_destroy(NvsCommonManager_t* manager);

/**
 * @brief 初始化通用NVS管理器
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_init(NvsCommonManager_t* manager);

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
                                      size_t size);

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
                                      size_t* size);

/**
 * @brief 保存字符串到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param str 字符串
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_save_str(NvsCommonManager_t* manager, 
                                     const char* key, 
                                     const char* str);

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
                                     size_t str_len);

/**
 * @brief 保存整数到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 整数值
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_save_u32(NvsCommonManager_t* manager, 
                                     const char* key, 
                                     uint32_t value);

/**
 * @brief 从NVS加载整数
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 整数值缓冲区
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_load_u32(NvsCommonManager_t* manager, 
                                     const char* key, 
                                     uint32_t* value);

/**
 * @brief 保存布尔值到NVS
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 布尔值
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_save_bool(NvsCommonManager_t* manager, 
                                      const char* key, 
                                      bool value);

/**
 * @brief 从NVS加载布尔值
 * @param manager 管理器实例句柄
 * @param key 键名
 * @param value 布尔值缓冲区
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_load_bool(NvsCommonManager_t* manager, 
                                      const char* key, 
                                      bool* value);

/**
 * @brief 从NVS删除数据
 * @param manager 管理器实例句柄
 * @param key 键名
 * @return ESP_OK 成功，其他失败
 */
esp_err_t nvs_common_manager_erase(NvsCommonManager_t* manager, const char* key);

/**
 * @brief 检查键是否存在
 * @param manager 管理器实例句柄
 * @param key 键名
 * @return 1 存在，0 不存在
 */
int nvs_common_manager_exists(NvsCommonManager_t* manager, const char* key);

/**
 * @brief 获取错误信息字符串
 * @param manager 管理器实例句柄
 * @param err 错误码
 * @return 错误信息字符串
 */
const char* nvs_common_manager_get_error_string(NvsCommonManager_t* manager, esp_err_t err);

#ifdef __cplusplus
}
#endif

#endif /* NVS_MANAGER_H */