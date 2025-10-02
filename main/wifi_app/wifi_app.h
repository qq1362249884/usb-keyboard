#ifndef MAIN_WIFI_APP_H_
#define MAIN_WIFI_APP_H_

/* 标准库头文件 */
#include <stdio.h>
#include <string.h>


/* ESP-IDF 核心头文件 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/lwip_napt.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_server.h"

/* nvs管理 */
#include "nvs_manager/unified_nvs_manager.h"
#include "spi_scanner/keymap_manager.h"

#include "spi_keyboard_config.h" // 合并后的SPI和按键映射配置文件

/* 全局NVS管理器声明 */
extern unified_nvs_manager_t *g_unified_nvs_manager;

/* 函数声明 */
void wifi_task(void);

/* WiFi控制函数 */
esp_err_t wifi_station_change(bool enable);
esp_err_t wifi_clear_password(void);
esp_err_t wifi_hotspot(void);

/* WiFi状态查询函数 */
bool wifi_is_connected(void);
esp_err_t wifi_get_mode(wifi_mode_t *mode);

/* NVS管理函数 */
esp_err_t wifi_app_nvs_init(void);


typedef struct {
    httpd_handle_t server;                  //服务器句柄
    
    esp_netif_t *sta_netif;                 // STA网络接口
    esp_netif_t *ap_netif;                  // AP网络接口
    char client_ip[16];                     // 客户端IP地址
    unified_nvs_manager_t *unified_nvs_manager; // 统一NVS管理器
    TaskHandle_t wifi_task_handle;           // WiFi任务句柄

    wifi_mode_t mode;              // WiFi模式
    bool wifi_enable_state;         //wifi启动状态
    uint32_t auto_shutdown_timer;   // 自动关闭计时器（秒）

    // 事件处理程序实例句柄
    esp_event_handler_instance_t wifi_event_handler_instance;
    esp_event_handler_instance_t ip_event_handler_instance;
} wifi_state_t;

extern wifi_state_t wifi_state;

#endif /* MAIN_WIFI_APP_H_ */