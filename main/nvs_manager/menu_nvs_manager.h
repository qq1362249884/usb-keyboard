#ifndef _MENU_NVS_MANAGER_H_
#define _MENU_NVS_MANAGER_H_

#include "nvs_manager.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 菜单NVS管理器句柄
 */
typedef struct MenuNvsManagerWrapper MenuNvsManager_t;

/**
 * @brief 创建菜单NVS管理器实例
 * @param namespace_name NVS命名空间名称（可选，NULL使用默认命名空间）
 * @param default_layer 默认映射层
 * @param default_ws2812_state 默认WS2812状态
 * @return 管理器实例句柄，失败返回NULL
 */
MenuNvsManager_t* menu_nvs_manager_create(const char* namespace_name, 
                                         uint8_t default_layer, 
                                         bool default_ws2812_state);

/**
 * @brief 销毁菜单NVS管理器实例
 * @param manager 管理器实例句柄
 */
void menu_nvs_manager_destroy(MenuNvsManager_t* manager);

/**
 * @brief 初始化菜单NVS管理器
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t menu_nvs_manager_init(MenuNvsManager_t* manager);

/**
 * @brief 保存当前映射层到NVS
 * @param manager 管理器实例句柄
 * @param layer 要保存的映射层索引
 * @return ESP_OK 成功，其他失败
 */
esp_err_t menu_nvs_manager_save_current_layer(MenuNvsManager_t* manager, uint8_t layer);

/**
 * @brief 从NVS加载当前映射层
 * @param manager 管理器实例句柄
 * @param layer 用于存储加载的映射层索引的指针
 * @return ESP_OK 成功，其他失败
 */
esp_err_t menu_nvs_manager_load_current_layer(MenuNvsManager_t* manager, uint8_t* layer);

/**
 * @brief 保存WS2812状态到NVS
 * @param manager 管理器实例句柄
 * @param state 要保存的WS2812状态（true为开启，false为关闭）
 * @return ESP_OK 成功，其他失败
 */
esp_err_t menu_nvs_manager_save_ws2812_state(MenuNvsManager_t* manager, bool state);

/**
 * @brief 从NVS加载WS2812状态
 * @param manager 管理器实例句柄
 * @param state 用于存储加载的WS2812状态的指针
 * @return ESP_OK 成功，其他失败
 */
esp_err_t menu_nvs_manager_load_ws2812_state(MenuNvsManager_t* manager, bool* state);

/**
 * @brief 保存所有菜单配置到NVS
 * @param manager 管理器实例句柄
 * @param layer 要保存的映射层索引
 * @param ws2812_state 要保存的WS2812状态
 * @return ESP_OK 成功，其他失败
 */
esp_err_t menu_nvs_manager_save_all(MenuNvsManager_t* manager, uint8_t layer, bool ws2812_state);

/**
 * @brief 从NVS加载所有菜单配置
 * @param manager 管理器实例句柄
 * @param layer 用于存储加载的映射层索引的指针
 * @param ws2812_state 用于存储加载的WS2812状态的指针
 * @return ESP_OK 成功，其他失败
 */
esp_err_t menu_nvs_manager_load_all(MenuNvsManager_t* manager, uint8_t* layer, bool* ws2812_state);

/**
 * @brief 重置所有菜单配置为默认值
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t menu_nvs_manager_reset_to_default(MenuNvsManager_t* manager);

#ifdef __cplusplus
}
#endif

#endif // _MENU_NVS_MANAGER_H_