#ifndef _OLED_MENU_EXAMPLE_H_
#define _OLED_MENU_EXAMPLE_H_

/* ==============================================
 * OLED菜单显示模块头文件
 * ==============================================
 * 功能：提供OLED菜单系统的公共接口
 * 作者：ESP32项目团队
 * 版本：v1.0
 * ==============================================*/

/* ======================================================
 * 头文件包含区域 - 按依赖层次组织
 * ======================================================*/

/* 标准C库头文件 */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* FreeRTOS相关头文件 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

/* ESP-IDF驱动头文件 */
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_log.h"

/* 项目内部模块头文件 */
#include "joystick.h"
#include "oled_menu.h"
#include "keyboard_led.h"
#include "wifi_app/wifi_app.h"           // WiFi功能接口
#include "spi_scanner/keymap_manager.h"   // 按键映射管理
#include "nvs_manager/menu_nvs_manager.h" // 菜单NVS存储管理
#include "oled_menu_wifi.h"              // WiFi相关动作函数
#include "keyboard_led/keyboard_led.h"

/* 图像数据声明 */
extern const uint8_t Image_setings[];

/* ======================================================
 * 宏定义区域
 * ======================================================*/

/* 任务配置宏 - 这些值在源文件中硬编码 */
/* 注意：实际的任务配置参数在oled_menu_example_start()函数中定义 */

/* ======================================================
 * 公共函数声明区域
 * ======================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 菜单系统入口函数
 * 
 * 初始化菜单系统，创建摇杆扫描任务和菜单显示任务
 * 该函数会启动两个FreeRTOS任务：
 * - joystick_task：处理摇杆输入事件
 * - menu_task：处理菜单显示和逻辑
 * 
 * @note 该函数应该在系统初始化完成后调用
 */
void oled_menu_example_start(void);

/**
 * @brief 清空按键事件队列
 * 
 * 移除队列中的所有按键事件，用于重置输入状态
 * 
 * @note 该函数是线程安全的
 */
void MenuManager_ClearKeyQueue(void);

/**
 * @brief 获取按键事件队列句柄
 * 
 * 用于外部模块访问按键事件队列
 * 
 * @return 按键事件队列句柄，如果未初始化则返回NULL
 */
QueueHandle_t get_key_queue(void);

/**
 * @brief 获取菜单管理器实例
 * 
 * 用于外部模块访问菜单管理器
 * 
 * @return 菜单管理器指针，如果未初始化则返回NULL
 */
MenuManager* get_menu_manager(void);

#ifdef __cplusplus
}
#endif

#endif /* _OLED_MENU_EXAMPLE_H_ */