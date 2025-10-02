/*
 * OLED菜单WiFi相关功能实现
 * 提供WiFi状态显示、开关控制、HTML网址显示和密码清除等功能
 */

// 标准库头文件
#include <stdbool.h>
#include <string.h>

// ESP-IDF组件头文件
#include "esp_wifi.h"
#include "esp_log.h"

// 项目内部头文件
#include "oled_menu_combined.h"
#include "oled_menu_display.h"
#include "wifi_app/wifi_app.h" // 访问WiFi功能接口

// 从oled_menu_display.c获取的函数声明
extern QueueHandle_t get_joystick_queue(void);
extern MenuManager* get_menu_manager(void);

/**
 * @brief 显示WiFi状态和详细信息（重构版，支持状态机模型）
 */
void menuActionWifiStatus(void) {
    uint8_t page = 0;          // 当前页码
    const uint8_t total_pages = 2; // 总页数
    bool exit_flag = false;    // 退出标志位
    
    while (!exit_flag) {
        OLED_Clear();
        
        // 获取WiFi状态结构体
        extern wifi_state_t wifi_state;
        
        // 检查WiFi是否已启动（基于WiFi任务句柄和模式）
        wifi_mode_t current_mode;
        bool wifi_enabled = (wifi_state.wifi_task_handle != NULL) && 
                           (esp_wifi_get_mode(&current_mode) == ESP_OK) && 
                           (current_mode != WIFI_MODE_NULL);
        
        if (wifi_enabled) {
            // 第一页：显示WiFi状态和模式
            if (page == 0) {
                // 标题栏
                OLED_ShowString(30, 0, "WiFi Info", OLED_6X8_HALF);
                
                // 显示WiFi状态（基于连接状态）
                char status_str[20];
                if (wifi_is_connected()) {
                    strcpy(status_str, "Status: Connected");
                } else if (current_mode & WIFI_MODE_STA) {
                    strcpy(status_str, "Status: Connecting");
                } else if (current_mode & WIFI_MODE_APSTA) {
                    strcpy(status_str, "Status: AP+STA");
                } else {
                    strcpy(status_str, "Status: Unknown");
                }
                OLED_ShowString(10, 9, status_str, OLED_6X8_HALF);
                
                // 显示WiFi模式（从底层获取）
                wifi_mode_t mode;
                char mode_str[20];
                if (esp_wifi_get_mode(&mode) == ESP_OK) {
                    if (mode == WIFI_MODE_STA) {
                        strcpy(mode_str, "Mode: STA");
                    } else if (mode == WIFI_MODE_APSTA) {
                        strcpy(mode_str, "Mode: AP+STA");
                    } else {
                        strcpy(mode_str, "Mode: Unknown");
                    }
                } else {
                    strcpy(mode_str, "Mode: Error");
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
                
                uint8_t ip_y_position = 17; // 默认IP地址位置
                
                // 显示当前IP地址
                OLED_ShowString(10, ip_y_position, "IP:", OLED_6X8_HALF);
                if (strlen(wifi_state.client_ip) > 0) {
                    OLED_ShowString(22, ip_y_position, wifi_state.client_ip, OLED_6X8_HALF);
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
            if (xQueueReceive(get_joystick_queue(), &key_event, 100 / portTICK_PERIOD_MS) == pdTRUE) {
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
 * @brief 切换WiFi开关状态（使用WiFi总开关函数，与初始化管理器保持一致）
 */
void menuActionWifiToggle(void) {
    OLED_Clear();
    
    // 获取WiFi状态结构体
    extern wifi_state_t wifi_state;
    
    // 优化状态判断逻辑：基于NVS中保存的持久化状态
    bool enable_wifi;
    
    // 检查WiFi任务是否正在运行
    if (wifi_state.wifi_task_handle != NULL) {
        // WiFi任务正在运行，说明WiFi已启用
        enable_wifi = false;  // 禁用WiFi
    } else {
        // WiFi任务未运行，说明WiFi已禁用
        enable_wifi = true;   // 启用WiFi
    }
    
    // 切换WiFi状态 - 使用与初始化管理器相同的WiFi总开关函数
    esp_err_t result;
    result = wifi_station_change(enable_wifi);
    
    // 显示切换结果
    OLED_ShowString(30, 0, "WiFi Toggle", OLED_6X8_HALF);
    
    if (result == ESP_OK) {
        if (enable_wifi) {
            OLED_ShowString(10, 18, "WiFi Enabled", OLED_6X8_HALF);
        } else {
            OLED_ShowString(10, 18, "WiFi Disabled", OLED_6X8_HALF);
        }
    } else {
        OLED_ShowString(10, 18, "Toggle Failed", OLED_6X8_HALF);
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
        extern wifi_state_t wifi_state;
        if (strlen(wifi_state.client_ip) > 0) {
            // 显示访问提示
            OLED_ShowString(10, 10, "Visit:", OLED_6X8_HALF);
            
            // 显示IP地址
            OLED_ShowString(10, 20, wifi_state.client_ip, OLED_6X8_HALF);
            
            // 显示端口号（如果不是默认的80端口）
            uint16_t port = 80; // 默认HTTP端口
            if (port != 80) {
                char port_str[10];
                sprintf(port_str, ":%d", port);
                // 计算IP地址的显示长度，确保端口号显示在合适的位置
                int ip_width = strlen(wifi_state.client_ip) * 6; // 每个字符6像素
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
    
    // 调用WiFi接口清除WiFi密码
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