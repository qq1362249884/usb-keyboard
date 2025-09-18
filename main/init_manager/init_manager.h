/*
 * 初始化管理器模块
 * 用于协调和管理不同硬件模块的初始化顺序和依赖关系
 */
#ifndef _INIT_MANAGER_H_
#define _INIT_MANAGER_H_

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// 模块ID枚举，用于标识不同的硬件/软件模块
typedef enum {
    MODULE_NVS = 0,          // NVS存储
    MODULE_WS2812,           // WS2812 LED
    MODULE_OLED,             // OLED显示
    MODULE_KEYBOARD,         // 键盘扫描
    MODULE_WIFI,             // WiFi模块
    MODULE_MAX               // 模块数量上限
} module_id_t;

// 初始化状态枚举
typedef enum {
    INIT_STATE_UNINITIALIZED = 0, // 未初始化
    INIT_STATE_IN_PROGRESS,       // 初始化中
    INIT_STATE_COMPLETED,         // 初始化完成
    INIT_STATE_FAILED             // 初始化失败
} init_state_t;

// 初始化回调函数类型
typedef esp_err_t (*init_func_t)(void);
// 配置应用回调函数类型
typedef esp_err_t (*apply_config_func_t)(void);

// 模块初始化描述结构体
typedef struct {
    module_id_t module_id;          // 模块ID
    init_func_t init_func;          // 初始化函数
    apply_config_func_t apply_config_func; // 配置应用函数
    module_id_t dependencies[MODULE_MAX]; // 依赖的模块ID列表
    int dependency_count;           // 依赖的模块数量
    init_state_t state;             // 当前初始化状态
    SemaphoreHandle_t ready_sem;    // 模块就绪信号量
} module_init_desc_t;

/**
 * @brief 初始化管理器
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_init(void);

/**
 * @brief 注册模块初始化描述符
 * @param desc 模块初始化描述符
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_register_module(const module_init_desc_t *desc);

/**
 * @brief 启动所有模块的初始化过程
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_start_init(void);

/**
 * @brief 等待指定模块初始化完成
 * @param module_id 模块ID
 * @param timeout_ms 超时时间(毫秒)
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_wait_for_module(module_id_t module_id, int timeout_ms);

/**
 * @brief 获取指定模块的初始化状态
 * @param module_id 模块ID
 * @param state 输出参数，用于存储模块状态
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_get_module_state(module_id_t module_id, init_state_t *state);

/**
 * @brief 应用指定模块的配置
 * @param module_id 模块ID
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_apply_module_config(module_id_t module_id);

/**
 * @brief 应用所有已初始化模块的配置
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_apply_all_configs(void);

#endif // _INIT_MANAGER_H_