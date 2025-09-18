#ifndef MAIN_WIFI_APP_H_
#define MAIN_WIFI_APP_H_

#include <stdio.h>    // 标准输入输出头文件
#include <string.h>   // 字符串处理函数头文件
#include "inttypes.h" // 固定宽度整数类型头文件

#include "esp_log.h" // ESP-IDF 日志功能头文件
#include "esp_err.h" // ESP-IDF 错误处理头文件

#include "nvs_flash.h" // 非易失性存储(NVS)闪存操作头文件

#include "esp_wifi.h"
#include "esp_compiler.h"       // ESP-IDF Wi-Fi 功能头文件
#include "esp_event.h"      // ESP-IDF 事件循环头文件
#include "esp_netif.h"      // ESP-IDF 网络接口头文件
#include "esp_mac.h"        // ESP-IDF MAC 地址操作头文件
#include "lwip/err.h"       // LWIP 错误处理头文件
#include "lwip/sys.h"       // LWIP 系统功能头文件
#include <esp_http_server.h> // HTTP服务器功能库
#include "esp_system.h"     // ESP系统功能头文件
#include <esp_http_server.h> // ESP-IDF HTTP服务器头文件
// #include <cJSON.h>            // ESP-IDF JSON解析库头文件



void wifi_task();
// esp_err_t wifi_sta_ap_init(); // 已废弃，使用wifi_task()替代
esp_err_t wifi_sta_init();
esp_err_t wifi_ap_init();

// 全局变量，用于存储AP的IP地址，供oled_menu_display.c访问
extern char client_ip[16];

// 获取当前WiFi模式
esp_err_t wifi_get_mode(wifi_mode_t *mode);

// 获取WiFi连接状态
bool wifi_is_connected(void);

// 获取AP模式下的SSID和密码
esp_err_t wifi_get_ap_info(char *ssid, size_t ssid_len, char *password, size_t password_len);

// 切换WiFi开关
esp_err_t wifi_toggle(bool enable);

// 获取HTTP服务器端口号
uint16_t wifi_get_http_port(void);

#endif /* MAIN_WIFI_APP_H_ */