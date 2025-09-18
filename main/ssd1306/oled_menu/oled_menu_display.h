#ifndef _OLED_MENU_EXAMPLE_H_
#define _OLED_MENU_EXAMPLE_H_

#include <string.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "driver/gpio.h"
#include "joystick.h"
#include "oled_menu.h"
#include "keyboard_led.h"

/**
 * @brief 菜单系统入口函数
 * 
 * 初始化菜单系统，创建摇杆扫描任务和菜单显示任务
 */
void oled_menu_example_start(void);

/**
 * @brief 清空按键事件队列
 * 
 * 移除队列中的所有按键事件
 */
void MenuManager_ClearKeyQueue(void);

#endif /* _OLED_MENU_EXAMPLE_H_ */