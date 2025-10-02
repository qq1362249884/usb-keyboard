#ifndef OLED_MENU_COMBINED_H
#define OLED_MENU_COMBINED_H

#include <stdbool.h>
#include "../oled_driver/OLED_driver.h"
/**
 * @brief 键盘相关功能声明
 */

/**
 * @brief 显示当前映射层信息
 * @param current_keymap_layer_val 当前映射层值
 */
void display_keymap_layer(int current_keymap_layer_val);

/**
 * @brief 检查自定义图层(层1)是否为空
 * @return true 层1映射为空（所有按键都为0）
 * @return false 层1映射不为空（至少有一个按键非0）
 */
bool is_layer1_empty(void);

/**
 * @brief 映射层菜单操作函数
 * 用于显示和切换当前的映射层
 */
void menuActionMappingLayer(void);

/**
 * @brief 灯效相关功能声明
 */

/**
 * @brief 切换灯效开关
 */
void menuActionRgbToggle(void);

/**
 * @brief 选择灯效模式
 */
void menuActionRgbModeSelect(void);

/**
 * @brief 调节灯效速度
 */
void menuActionRgbSpeedAdjust(void);

/**
 * @brief HSV调控统一函数
 * 集成色调(0-360)、饱和度(0-100%)和亮度(0-100%)的调节
 */
void menuActionRgbHsvAdjust(void);

/**
 * @brief 计算器相关功能声明
 */

/**
 * @brief 计算器功能
 * 使用键盘按键进行输入，在使用计算器时中断tinyusb的发送报告
 */
void menuActionCalculator(void);

/**
 * @brief WiFi相关功能声明
 */

/**
 * @brief 显示WiFi状态和详细信息（支持摇杆滚动查看）
 */
void menuActionWifiStatus(void);

/**
 * @brief 切换WiFi开关
 */
void menuActionWifiToggle(void);

/**
 * @brief 显示HTML网址
 */
void menuActionHtmlUrl(void);

/**
 * @brief 清除WiFi密码动作函数
 */
void menuActionClearWifiPassword(void);

#endif /* OLED_MENU_COMBINED_H */