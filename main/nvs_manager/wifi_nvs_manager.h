#ifndef _WIFI_NVS_MANAGER_H_
#define _WIFI_NVS_MANAGER_H_

#include "nvs_manager.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 不透明句柄类型
typedef struct WifiNvsManagerWrapper WifiNvsManager_t;

/**
 * @brief 创建WiFi NVS管理器实例
 * @return 管理器实例句柄
 */
WifiNvsManager_t* wifi_nvs_manager_create(void);

/**
 * @brief 销毁WiFi NVS管理器实例
 * @param manager 管理器实例句柄
 */
void wifi_nvs_manager_destroy(WifiNvsManager_t* manager);

/**
 * @brief 初始化WiFi NVS管理器
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t wifi_nvs_manager_init(WifiNvsManager_t* manager);

/**
 * @brief 保存WiFi配置到NVS
 * @param manager 管理器实例句柄
 * @param ssid WiFi SSID
 * @param password WiFi密码
 * @return ESP_OK 成功，其他失败
 */
esp_err_t wifi_nvs_manager_save_config(WifiNvsManager_t* manager, 
                                      const char* ssid, 
                                      const char* password);

/**
 * @brief 从NVS加载WiFi配置
 * @param manager 管理器实例句柄
 * @param ssid WiFi SSID缓冲区
 * @param ssid_len SSID缓冲区大小
 * @param password WiFi密码缓冲区
 * @param password_len 密码缓冲区大小
 * @return ESP_OK 成功，其他失败
 */
esp_err_t wifi_nvs_manager_load_config(WifiNvsManager_t* manager, 
                                      char* ssid, 
                                      size_t ssid_len, 
                                      char* password, 
                                      size_t password_len);

/**
 * @brief 检查WiFi配置是否存在
 * @param manager 管理器实例句柄
 * @return true 存在，false 不存在
 */
bool wifi_nvs_manager_has_config(WifiNvsManager_t* manager);

/**
 * @brief 清除WiFi配置
 * @param manager 管理器实例句柄
 * @return ESP_OK 成功，其他失败
 */
esp_err_t wifi_nvs_manager_clear_config(WifiNvsManager_t* manager);

#ifdef __cplusplus
}
#endif

#endif // _WIFI_NVS_MANAGER_H_