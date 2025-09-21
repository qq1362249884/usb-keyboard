/*
 * OLED菜单WiFi相关功能实现
 * 提供WiFi状态显示、开关控制、HTML网址显示和密码清除等功能
 */

// 标准库头文件
#include <stdbool.h>
#include <string.h>

// ESP-IDF组件头文件
#include "esp_wifi.h"

// 项目内部头文件
#include "oled_menu_wifi.h"
#include "oled_menu_display.h"
#include "oled_menu.h"
#include "wifi_app/wifi_app.h" // 访问client_ip全局变量和WiFi功能
#include "nvs_manager/menu_nvs_manager.h" // 菜单NVS管理器

// 从oled_menu_display.c获取的函数声明
extern QueueHandle_t get_key_queue(void);
extern MenuManager* get_menu_manager(void);

/**
 * @brief 显示WiFi状态和详细信息（支持摇杆滚动查看）
 */
void menuActionWifiStatus(void) {
    uint8_t page = 0;          // 当前页码
    const uint8_t total_pages = 2; // 总页数
    bool exit_flag = false;    // 退出标志位
    
    while (!exit_flag) {
        OLED_Clear();
        
        // 检查WiFi是否已启动
        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) == ESP_OK && mode != WIFI_MODE_NULL) {
            // 第一页：显示WiFi状态和模式
            if (page == 0) {
                // 标题栏
                OLED_ShowString(30, 0, "WiFi Info", OLED_6X8_HALF);
                
                // 显示WiFi状态
                OLED_ShowString(10, 9, "Status: On", OLED_6X8_HALF);
                
                // 显示具体模式
                char mode_str[20];
                if (mode == WIFI_MODE_AP) {
                    strcpy(mode_str, "Mode: AP");
                } else if (mode == WIFI_MODE_STA) {
                    strcpy(mode_str, "Mode: STA");
                } else if (mode == WIFI_MODE_APSTA) {
                    strcpy(mode_str, "Mode: AP+STA");
                }
                OLED_ShowString(10, 17, mode_str, OLED_6X8_HALF);
            }
            // 第二页：显示连接状态和详细信息
            else if (page == 1) {
                // 标题栏
                OLED_ShowString(30, 0, "WiFi Info", OLED_6X8_HALF);
                
                // 显示连接状态
                if (wifi_is_connected()) {
                    OLED_ShowString(10, 9, "Connected", OLED_6X8_HALF);
                } else {
                    OLED_ShowString(10, 9, "Disconnected", OLED_6X8_HALF);
                }
                
                // 获取并显示AP信息
                char ssid[32] = {0};
                char password[64] = {0};
                uint8_t ip_y_position = 17; // 默认IP地址位置
                
                if (wifi_get_ap_info(ssid, sizeof(ssid), password, sizeof(password)) == ESP_OK) {
                    // 在AP模式下显示AP信息
                    if (mode & WIFI_MODE_AP) {
                        OLED_ShowString(10, 17, "AP:", OLED_6X8_HALF);
                        // 截断过长的SSID以确保显示完整
                        if (strlen(ssid) > 15) {
                            char truncated_ssid[16];
                            strncpy(truncated_ssid, ssid, 15);
                            truncated_ssid[15] = '\0';
                            OLED_ShowString(22, 17, truncated_ssid, OLED_6X8_HALF);
                        } else {
                            OLED_ShowString(22, 17, ssid, OLED_6X8_HALF);
                        }
                        // 在AP模式下，IP地址下移
                        ip_y_position = 25;
                    }
                }
                
                // 显示当前IP地址
                OLED_ShowString(10, ip_y_position, "IP:", OLED_6X8_HALF);
                if (strlen(client_ip) > 0) {
                    OLED_ShowString(22, ip_y_position, client_ip, OLED_6X8_HALF);
                } else {
                    OLED_ShowString(22, ip_y_position, "0.0.0.0", OLED_6X8_HALF);
                }
            }
        } else {
            // WiFi未启用时显示简单信息
            OLED_ShowString(30, 0, "WiFi Info", OLED_6X8_HALF);
            OLED_ShowString(10, 18, "WiFi is Off", OLED_6X8_HALF);
        }
        
        // 显示翻页提示（如果有多页）
        if (total_pages > 1) {
            char page_info[10];
            sprintf(page_info, "%d/%d", page + 1, total_pages);
            OLED_ShowString(95, 0, page_info, OLED_6X8_HALF);
        }
        
        OLED_Update();
        
        // 等待摇杆操作或超时（3秒）
        uint8_t key_event = 0;
        TickType_t start_time = xTaskGetTickCount();
        const TickType_t timeout = 3000 / portTICK_PERIOD_MS;
        
        while ((xTaskGetTickCount() - start_time) < timeout) {
            if (xQueueReceive(get_key_queue(), &key_event, 100 / portTICK_PERIOD_MS) == pdTRUE) {
                switch (key_event) {
                    case MENU_OP_UP:
                        // 上翻页（循环）
                        page = (page > 0) ? (page - 1) : (total_pages - 1);
                        break;
                    case MENU_OP_DOWN:
                        // 下翻页（循环）
                        page = (page < total_pages - 1) ? (page + 1) : 0;
                        break;
                    case MENU_OP_ENTER:
                    case MENU_OP_BACK:
                        // 退出显示
                        exit_flag = true;
                        break;
                    default:
                        break;
                }
                break; // 有按键事件，跳出循环刷新显示
            }
        }
    }
    
    // 返回到主菜单
    MenuManager_DisplayMenu(get_menu_manager(), 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 切换WiFi开关状态
 *        开启状态下切换为关闭，关闭状态下切换为开启
 */
void menuActionWifiToggle(void) {
    OLED_Clear();
    
    // 检查当前WiFi状态
    wifi_mode_t current_mode;
    esp_err_t err = esp_wifi_get_mode(&current_mode);
    
    if (err != ESP_OK) {
        // 获取WiFi模式失败
        OLED_ShowString(10, 10, "Get Mode Failed", OLED_8X16_HALF);
        OLED_Update();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        MenuManager_DisplayMenu(get_menu_manager(), 0, 0, OLED_8X16_HALF);
        return;
    }
    
    // 确定要切换到的新状态（当前关闭则开启，当前开启则关闭）
    bool new_state = (current_mode == WIFI_MODE_NULL);
    err = wifi_toggle(new_state);
    
    // 重新获取WiFi状态以确认操作结果
    wifi_mode_t updated_mode;
    esp_wifi_get_mode(&updated_mode);
    
    // WiFi状态的保存现在由wifi_toggle函数内部处理
    
    // 显示操作结果
    if (err == ESP_OK) {
        if (updated_mode == WIFI_MODE_NULL) {
            OLED_ShowString(10, 10, "WiFi Disabled", OLED_8X16_HALF);
        } else {
            OLED_ShowString(10, 10, "WiFi Enabled", OLED_8X16_HALF);
        }
    } else {
        OLED_ShowString(10, 10, "Toggle Failed", OLED_8X16_HALF);
    }
    
    OLED_Update();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    // 返回到主菜单
    MenuManager_DisplayMenu(get_menu_manager(), 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 显示HTML服务器访问网址
 *        显示设备的IP地址和端口号，用于在浏览器中访问设备的Web界面
 */
void menuActionHtmlUrl(void) {
    OLED_Clear();
    
    // 标题栏
    OLED_ShowString(30, 0, "HTML URL", OLED_6X8_HALF);
    
    // 检查WiFi是否已启动
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK && mode != WIFI_MODE_NULL) {
        
        // 检查是否有有效IP地址
        if (strlen(client_ip) > 0) {
            // 显示访问提示
            OLED_ShowString(10, 10, "Visit:", OLED_6X8_HALF);
            
            // 显示IP地址
            OLED_ShowString(10, 20, client_ip, OLED_6X8_HALF);
            
            // 显示端口号（如果不是默认的80端口）
            uint16_t port = wifi_get_http_port();
            if (port != 80) {
                char port_str[10];
                sprintf(port_str, ":%d", port);
                // 计算IP地址的显示长度，确保端口号显示在合适的位置
                int ip_width = strlen(client_ip) * 6; // 每个字符6像素
                OLED_ShowString(10 + ip_width, 20, port_str, OLED_6X8_HALF);
            }
            
            // 显示浏览器访问提示
            OLED_ShowString(10, 34, "In browser", OLED_6X8_HALF);
        } else {
            // IP地址未分配
            OLED_ShowString(10, 10, "IP: 0.0.0.0", OLED_8X16_HALF);
        }
        
    } else {
        // WiFi未开启
        OLED_ShowString(10, 10, "WiFi is Off", OLED_8X16_HALF);
    }
    
    OLED_Update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // 返回到主菜单
    MenuManager_DisplayMenu(get_menu_manager(), 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 清除WiFi密码配置
 *        清除保存的WiFi连接信息，并将WiFi模式设置为APSTA模式
 */
void menuActionClearWifiPassword(void) {
    OLED_Clear();
    
    // 显示操作标题
    OLED_ShowString(10, 8, "Clear WiFi PW", OLED_6X8_HALF);
    
    // 调用wifi_clear_password函数清除WiFi配置
    esp_err_t err = wifi_clear_password();
    
    // 显示操作结果
    if (err == ESP_OK) {
        OLED_ShowString(10, 16, "Success", OLED_6X8_HALF);
        OLED_ShowString(10, 24, "APSTA Mode", OLED_6X8_HALF);
    } else {
        OLED_ShowString(10, 16, "Failed", OLED_6X8_HALF);
    }
    
    OLED_Update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // 返回到主菜单
    MenuManager_DisplayMenu(get_menu_manager(), 0, 0, OLED_8X16_HALF);
}