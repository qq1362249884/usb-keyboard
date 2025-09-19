#ifndef MAIN_WIFI_APP_H_
#define MAIN_WIFI_APP_H_

/* 标准库头文件 */
#include <stdio.h>
#include <string.h>
#include "inttypes.h"

/* ESP-IDF 核心头文件 */
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_compiler.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "freertos/task.h"

/* LWIP 网络库头文件 */
#include "lwip/err.h"
#include "lwip/sys.h"

/* nvs管理 */
#include "nvs_manager/wifi_nvs_manager.h"
#include "nvs_manager/menu_nvs_manager.h"
#include "spi_scanner/keymap_manager.h"

/* 函数声明 */
void wifi_task(void);

/* 获取当前WiFi模式 */
esp_err_t wifi_get_mode(wifi_mode_t *mode);

/* 获取WiFi连接状态 */
bool wifi_is_connected(void);

/* 获取AP模式下的SSID和密码 */
esp_err_t wifi_get_ap_info(char *ssid, size_t ssid_len, char *password, size_t password_len);

/* 切换WiFi开关 */
esp_err_t wifi_toggle(bool enable);

/* 获取HTTP服务器端口号 */
uint16_t wifi_get_http_port(void);

/* 清除保存的WiFi密码 */
esp_err_t wifi_clear_password(void);

/* 全局变量 - 用于存储客户端IP地址，供oled_menu_display.c访问 */
extern char client_ip[16];

#endif /* MAIN_WIFI_APP_H_ */