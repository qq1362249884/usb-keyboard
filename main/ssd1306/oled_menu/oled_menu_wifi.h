#ifndef OLED_MENU_WIFI_H
#define OLED_MENU_WIFI_H

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

#endif /* OLED_MENU_WIFI_H */