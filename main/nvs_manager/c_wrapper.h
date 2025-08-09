#ifndef _C_WRAPPER_H_
#define _C_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief C语言包装器头文件
 * 
 * 这个文件提供了C语言接口来调用C++的NVS管理器类
 */

// 不透明句柄类型
typedef struct KeymapNvsManagerWrapper KeymapNvsManager_t;

/**
 * @brief 创建NVS管理器实例
 * @param namespace_name NVS命名空间名称
 * @param key_prefix 键名前缀
 * @param num_keys 按键数量
 * @param num_layers 层数
 * @return 管理器实例句柄
 */
KeymapNvsManager_t* keymap_nvs_manager_create(const char* namespace_name, 
                                               const char* key_prefix, 
                                               uint8_t num_keys, 
                                               uint8_t num_layers);

/**
 * @brief 销毁NVS管理器实例
 * @param manager 管理器实例句柄
 */
void keymap_nvs_manager_destroy(KeymapNvsManager_t* manager);

/**
 * @brief 初始化NVS管理器
 * @param manager 管理器实例句柄
 * @param default_keymaps 默认按键映射数组
 * @param keymaps 运行时按键映射数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t keymap_nvs_manager_init(KeymapNvsManager_t* manager, 
                                   const uint16_t* default_keymaps, 
                                   uint16_t* keymaps);

/**
 * @brief 保存按键映射
 * @param manager 管理器实例句柄
 * @param layer 层索引
 * @param keymap 按键映射数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t keymap_nvs_manager_save(KeymapNvsManager_t* manager, 
                                   uint8_t layer, 
                                   const uint16_t* keymap);

/**
 * @brief 加载按键映射
 * @param manager 管理器实例句柄
 * @param layer 层索引
 * @param keymap 用于存储按键映射的数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t keymap_nvs_manager_load(KeymapNvsManager_t* manager, 
                                   uint8_t layer, 
                                   uint16_t* keymap);

/**
 * @brief 重置按键映射为默认值
 * @param manager 管理器实例句柄
 * @param layer 层索引
 * @return ESP_OK 成功，其他失败
 */
esp_err_t keymap_nvs_manager_reset(KeymapNvsManager_t* manager, uint8_t layer);

/**
 * @brief 保存所有层的按键映射
 * @param manager 管理器实例句柄
 * @param keymaps 按键映射数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t keymap_nvs_manager_save_all(KeymapNvsManager_t* manager, const uint16_t* keymaps);

/**
 * @brief 加载所有层的按键映射
 * @param manager 管理器实例句柄
 * @param keymaps 用于存储按键映射的数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t keymap_nvs_manager_load_all(KeymapNvsManager_t* manager, uint16_t* keymaps);

/**
 * @brief 检查按键映射是否存在
 * @param manager 管理器实例句柄
 * @param layer 层索引
 * @return 1 存在，0 不存在
 */
int keymap_nvs_manager_exists(KeymapNvsManager_t* manager, uint8_t layer);

/**
 * @brief 测试按键映射配置
 * @param manager 管理器实例句柄
 * @param keymaps 按键映射数组
 */
void keymap_nvs_manager_test_config(KeymapNvsManager_t* manager, uint16_t* keymaps);

/**
 * @brief 获取错误信息字符串
 * @param manager 管理器实例句柄
 * @param err 错误码
 * @return 错误信息字符串
 */
const char* keymap_nvs_manager_get_error_string(KeymapNvsManager_t* manager, esp_err_t err);

#ifdef __cplusplus
}
#endif

#endif // _C_WRAPPER_H_