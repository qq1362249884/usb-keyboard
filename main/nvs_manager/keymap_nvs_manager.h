#ifndef _KEYMAP_NVS_MANAGER_H_
#define _KEYMAP_NVS_MANAGER_H_

#include "nvs_manager.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 按键映射NVS管理器句柄
 */
typedef struct KeymapNvsManagerWrapper KeymapNvsManager_t;

/**
 * @brief 创建按键映射NVS管理器实例
 * @param namespace_name NVS命名空间名称
 * @param key_prefix 键名前缀
 * @param num_keys 按键数量
 * @param num_layers 层数
 * @param default_keymaps 默认按键映射数组
 * @return 管理器实例句柄
 */
KeymapNvsManager_t* keymap_nvs_manager_create(const char* namespace_name, 
                                              const char* key_prefix, 
                                              uint8_t num_keys, 
                                              uint8_t num_layers, 
                                              const uint16_t* default_keymaps);

/**
 * @brief 销毁按键映射NVS管理器实例
 * @param manager 管理器实例句柄
 */
void keymap_nvs_manager_destroy(KeymapNvsManager_t* manager);

/**
 * @brief 初始化按键映射NVS管理器
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t keymap_nvs_manager_init(KeymapNvsManager_t* manager);

/**
 * @brief 保存指定层的按键映射
 * @param manager 管理器实例句柄
 * @param layer 层索引
 * @param keymap 按键映射数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t keymap_nvs_manager_save(KeymapNvsManager_t* manager, 
                                 uint8_t layer, 
                                 const uint16_t* keymap);

/**
 * @brief 加载指定层的按键映射
 * @param manager 管理器实例句柄
 * @param layer 层索引
 * @param keymap 用于存储按键映射的数组
 * @return ESP_OK 成功，其他失败
 */
esp_err_t keymap_nvs_manager_load(KeymapNvsManager_t* manager, 
                                 uint8_t layer, 
                                 uint16_t* keymap);

/**
 * @brief 重置指定层的按键映射为默认值
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
 * @brief 检查指定层的按键映射是否存在
 * @param manager 管理器实例句柄
 * @param layer 层索引
 * @return 1 存在，0 不存在
 */
int keymap_nvs_manager_exists(KeymapNvsManager_t* manager, uint8_t layer);

/**
 * @brief 获取按键数量
 * @param manager 管理器实例句柄
 * @return 按键数量
 */
uint8_t keymap_nvs_manager_get_num_keys(KeymapNvsManager_t* manager);

/**
 * @brief 获取层数
 * @param manager 管理器实例句柄
 * @return 层数
 */
uint8_t keymap_nvs_manager_get_num_layers(KeymapNvsManager_t* manager);

/**
 * @brief 测试按键映射配置功能
 * @param manager 管理器实例句柄
 * @param keymaps 运行时按键映射数组
 */
void keymap_nvs_manager_test_config(KeymapNvsManager_t* manager, uint16_t* keymaps);

#ifdef __cplusplus
}
#endif

#endif // _KEYMAP_NVS_MANAGER_H_