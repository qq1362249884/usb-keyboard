#include "oled_menu_display.h"
#include "oled_menu.h"
#include <stdbool.h>
#include "wifi_app/wifi_app.h" // 包含wifi_app头文件以访问client_ip全局变量和WiFi功能
#include "spi_scanner/keymap_manager.h" // 包含按键映射管理器头文件
#include "nvs_manager/menu_nvs_manager.h" // 包含菜单NVS管理器头文件
#include "esp_wifi.h" // 包含WiFi相关定义

// 按键状态队列
static QueueHandle_t keyQueue = NULL;

// 菜单管理器实例
static MenuManager menuManager;

// 全局变量用于存储当前映射层
uint8_t current_keymap_layer = 0; // 默认使用层0

// 全局菜单NVS管理器句柄
static MenuNvsManager_t* g_menu_nvs_manager = NULL;

/**
 * @brief 初始化菜单NVS管理器
 * @return ESP_OK 成功，其他失败
 */
static esp_err_t menu_nvs_init(void) {
    // 如果NVS管理器已存在，先销毁
    if (g_menu_nvs_manager) {
        menu_nvs_manager_destroy(g_menu_nvs_manager);
        g_menu_nvs_manager = NULL;
    }
    
    // 创建菜单NVS管理器实例，默认层为0，默认WS2812状态为false，默认WiFi状态为false
    g_menu_nvs_manager = menu_nvs_manager_create(NULL, 0, false);
    if (!g_menu_nvs_manager) {
        ESP_LOGE("MENU_NVS", "Failed to create menu NVS manager");
        return ESP_FAIL;
    }
    
    // 初始化管理器
    esp_err_t err = menu_nvs_manager_init(g_menu_nvs_manager);
    if (err != ESP_OK) {
        ESP_LOGE("MENU_NVS", "Failed to initialize menu NVS manager");
        menu_nvs_manager_destroy(g_menu_nvs_manager);
        g_menu_nvs_manager = NULL;
        return err;
    }
    
    return ESP_OK;
}

/**
 * @brief 从NVS加载菜单配置
 */
static void load_menu_config(void) {
    if (!g_menu_nvs_manager) {
        if (menu_nvs_init() != ESP_OK) {
            ESP_LOGE("MENU_NVS", "Failed to initialize NVS manager during config load");
            return;
        }
    }
    
    // 记录加载前的状态
    uint8_t prev_layer = current_keymap_layer;
    bool prev_ws2812_state = kob_ws2812_is_enable();
    bool prev_wifi_state = false;
    wifi_mode_t current_mode;
    if (esp_wifi_get_mode(&current_mode) == ESP_OK) {
        prev_wifi_state = (current_mode != WIFI_MODE_NULL);
    }
    
    ESP_LOGI("MENU_NVS", "Before loading config: layer=%d, ws2812_state=%d, wifi_state=%d", 
             prev_layer, prev_ws2812_state, prev_wifi_state);
    
    // 准备加载的变量
    uint8_t layer = current_keymap_layer;
    bool ws2812_state = prev_ws2812_state;
    bool wifi_state = prev_wifi_state;
    
    // 加载所有菜单配置
    esp_err_t err = menu_nvs_manager_load_all(g_menu_nvs_manager, &layer, &ws2812_state, &wifi_state);
    
    // 无论加载结果如何，都应用合理的值
    if (err == ESP_OK || err == ESP_ERR_NOT_FOUND) {
        // 确保加载的值在有效范围内
        if (layer <= 1) {
            current_keymap_layer = layer;
        } else {
            ESP_LOGW("MENU_NVS", "Loaded layer %d is out of range, keeping current layer %d", 
                     layer, current_keymap_layer);
        }
        
        // 强制设置WS2812状态 - 关键修改点
        ESP_LOGI("MENU_NVS", "Applying WS2812 state from NVS: %d", ws2812_state);
        
        // 添加短暂延时确保LED驱动初始化完成
        vTaskDelay(50 / portTICK_PERIOD_MS);
        
        // 应用WS2812状态，并添加重试机制
        bool state_applied = false;
        int retry_count = 0;
        const int max_retries = 3;
        
        while (!state_applied && retry_count < max_retries) {
            kob_ws2812_enable(ws2812_state);
            
            // 短暂延时让状态生效
            vTaskDelay(10 / portTICK_PERIOD_MS);
            
            // 再次检查状态是否正确应用
            bool applied_state = kob_ws2812_is_enable();
            ESP_LOGI("MENU_NVS", "WS2812 state after application (attempt %d): %d", 
                     retry_count + 1, applied_state);
            
            if (applied_state == ws2812_state) {
                state_applied = true;
            } else {
                ESP_LOGW("MENU_NVS", "WS2812 state mismatch after application! Expected: %d, Actual: %d", 
                         ws2812_state, applied_state);
                retry_count++;
                vTaskDelay(10 / portTICK_PERIOD_MS); // 再次尝试前的延时
            }
        }
        
        if (!state_applied) {
            ESP_LOGE("MENU_NVS", "Failed to apply WS2812 state after %d attempts", max_retries);
        }
    } else {
        ESP_LOGE("MENU_NVS", "Failed to load menu config: %s", esp_err_to_name(err));
        // 即使加载失败，我们也尝试确保WS2812状态已知
        
        // 添加短暂延时确保LED驱动初始化完成
        vTaskDelay(50 / portTICK_PERIOD_MS);
        
        kob_ws2812_enable(ws2812_state);
    }
    
    // 应用WiFi状态
    ESP_LOGI("MENU_NVS", "Applying WiFi state from NVS: %d", wifi_state);
    
    // 检查当前WiFi状态
    if (esp_wifi_get_mode(&current_mode) == ESP_OK) {
        bool current_wifi_state = (current_mode != WIFI_MODE_NULL);
        
        // 只有当状态不一致时才切换WiFi
        if (current_wifi_state != wifi_state) {
            // 添加延迟，确保WiFi任务初始化完成
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            
            // 再次检查WiFi模式，确保没有其他操作正在修改它
            if (esp_wifi_get_mode(&current_mode) == ESP_OK) {
                // 再次确认当前状态
                current_wifi_state = (current_mode != WIFI_MODE_NULL);
                
                if (current_wifi_state != wifi_state) {
                    esp_err_t wifi_err = wifi_toggle(wifi_state);
                    if (wifi_err != ESP_OK) {
                        ESP_LOGE("MENU_NVS", "Failed to apply WiFi state: %s", esp_err_to_name(wifi_err));
                        // 不要让错误导致系统崩溃，只记录日志
                    }
                }
            }
        }
    }
    
    // 记录加载后的状态
    ESP_LOGI("MENU_NVS", "After loading config: layer=%d, ws2812_state=%d, wifi_state=%d", 
             current_keymap_layer, kob_ws2812_is_enable(), 
             ((esp_wifi_get_mode(&current_mode) == ESP_OK) && (current_mode != WIFI_MODE_NULL)));
}

/**
 * @brief 保存菜单配置到NVS
 */
static void save_menu_config(void) {
    if (!g_menu_nvs_manager) {
        if (menu_nvs_init() != ESP_OK) {
            return;
        }
    }
    
    // 获取当前WS2812状态
    bool ws2812_state = kob_ws2812_is_enable();
    
    // 获取当前WiFi状态
    bool wifi_state = false;
    wifi_mode_t current_mode;
    if (esp_wifi_get_mode(&current_mode) == ESP_OK) {
        wifi_state = (current_mode != WIFI_MODE_NULL);
    }
    
    // 保存所有菜单配置
    menu_nvs_manager_save_all(g_menu_nvs_manager, current_keymap_layer, ws2812_state, wifi_state);
}

/**
 * @brief 示例菜单操作函数
 */
// static void menu_loding(void){
//     
// }

static void menuAction1(void) {
    OLED_Clear();
    OLED_ShowString(10, 10, "Action 1 executed", OLED_8X16_HALF);
    OLED_Update();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

static void menuAction2(void) {
    OLED_Clear();
    OLED_ShowString(10, 10, "Action 2 executed", OLED_8X16_HALF);
    OLED_Update();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

static void menuAction3(void) {
    OLED_Clear();
    
    // 检查client_ip是否有有效数据
    if (strlen(client_ip) > 0) {
        OLED_ShowString(10, 10, "AP_IP:", OLED_6X8_HALF);
        OLED_ShowString(10, 20, client_ip, OLED_6X8_HALF);
    } else {
        OLED_ShowString(10, 10, "AP_IP:", OLED_6X8_HALF);
        OLED_ShowString(10, 20, "0.0.0.0", OLED_6X8_HALF);
    }
    
    OLED_Update();
    vTaskDelay(2000 / portTICK_PERIOD_MS); // 显示2秒
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

static void menuAction4(void){
    OLED_Clear();
    if(kob_ws2812_is_enable() == false){
        kob_ws2812_enable(true);
        OLED_ShowString(10, 10, "ws2812_enable", OLED_8X16_HALF);
    }else{
        kob_ws2812_enable(false);
        OLED_ShowString(10, 10, "ws2812_false", OLED_8X16_HALF);
    }
    OLED_Update();
    
    // 保存WS2812状态到NVS
    save_menu_config();
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 显示WiFi状态和详细信息（支持摇杆滚动查看）
 */
static void menuActionWifiStatus(void) {
    // 定义显示页面状态
    uint8_t page = 0;
    const uint8_t total_pages = 2; // 总页数
    bool exit_flag = false;
    
    while (!exit_flag) {
        OLED_Clear();
        
        // 检查WiFi是否已启动
        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) == ESP_OK && mode != WIFI_MODE_NULL) {
            // 第一页：标题、状态和模式
            if (page == 0) {
                // 标题栏 - 使用OLED_6X8_HALF字体
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
            // 第二页：连接状态和详细信息
            else if (page == 1) {
                // 标题栏 - 使用OLED_6X8_HALF字体
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
                
                // 显示当前IP地址，根据是否在AP模式调整Y坐标
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
        
        // 如果有多页，显示翻页提示
        if (total_pages > 1) {
            char page_info[10];
            sprintf(page_info, "%d/%d", page + 1, total_pages);
            OLED_ShowString(95, 0, page_info, OLED_6X8_HALF);
        }
        
        OLED_Update();
        
        // 等待摇杆操作
        uint8_t key_event = 0;
        TickType_t start_time = xTaskGetTickCount();
        
        // 等待3秒或直到有按键事件
        while ((xTaskGetTickCount() - start_time) < 3000 / portTICK_PERIOD_MS) {
            if (xQueueReceive(keyQueue, &key_event, 100 / portTICK_PERIOD_MS) == pdTRUE) {
                switch (key_event) {
                    case MENU_OP_UP:
                        // 上翻页
                        if (page > 0) {
                            page--;
                        } else {
                            page = total_pages - 1;
                        }
                        break;
                    case MENU_OP_DOWN:
                        // 下翻页
                        if (page < total_pages - 1) {
                            page++;
                        } else {
                            page = 0;
                        }
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
    
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 切换WiFi开关
 */
static void menuActionWifiToggle(void) {
    OLED_Clear();
    
    // 检查当前WiFi状态
    wifi_mode_t current_mode;
    esp_err_t err = esp_wifi_get_mode(&current_mode);
    
    if (err != ESP_OK) {
        OLED_ShowString(10, 10, "Get Mode Failed", OLED_8X16_HALF);
        OLED_Update();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
        return;
    }
    
    // 确定要切换到的新状态
    bool new_state = (current_mode == WIFI_MODE_NULL);
    err = wifi_toggle(new_state);
    
    // 重新获取WiFi状态以确认操作结果
    wifi_mode_t updated_mode;
    esp_wifi_get_mode(&updated_mode);
    
    // 注意：WiFi状态的保存现在由wifi_toggle函数内部处理，不再需要这里保存
    
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
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}



/**
 * @brief 显示HTML网址
 */
static void menuActionHtmlUrl(void) {
    OLED_Clear();
    
    // 标题栏
    OLED_ShowString(30, 0, "HTML URL", OLED_6X8_HALF);
    
    // 检查WiFi是否已启动并获取IP
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK && mode != WIFI_MODE_NULL) {
        
        if (strlen(client_ip) > 0) {
            // 简化显示，只显示关键信息
            OLED_ShowString(10, 10, "Visit:", OLED_6X8_HALF);
            
            // 显示IP地址（使用更小的字体）
            OLED_ShowString(10, 20, client_ip, OLED_6X8_HALF);
            
            // 显示端口号（如果不是80）
            uint16_t port = wifi_get_http_port();
            if (port != 80) {
                char port_str[10];
                sprintf(port_str, ":%d", port);
                // 计算IP地址的显示长度，确保端口号显示在合适的位置
                int ip_width = strlen(client_ip) * 6; // 每个字符6像素
                OLED_ShowString(10 + ip_width, 20, port_str, OLED_6X8_HALF);
            }
            
            // 在下方显示简短提示
            OLED_ShowString(10, 34, "In browser", OLED_6X8_HALF);
        } else {
            OLED_ShowString(10, 10, "IP: 0.0.0.0", OLED_8X16_HALF);
        }
        
    } else {
        OLED_ShowString(10, 10, "WiFi is Off", OLED_8X16_HALF);
    }
    
    OLED_Update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 清除WiFi密码动作函数
 */
static void menuActionClearWifiPassword(void) {
    OLED_Clear();
    OLED_ShowString(10, 10, "Clear WiFi PW", OLED_8X16_HALF);
    
    // Call wifi_clear_password to clear WiFi config
    esp_err_t err = wifi_clear_password();
    
    // Show operation result
    if (err == ESP_OK) {
        OLED_ShowString(10, 26, "Success", OLED_8X16_HALF);
        OLED_ShowString(10, 32, "APSTA Mode", OLED_6X8_HALF);
    } else {
        OLED_ShowString(10, 26, "Failed", OLED_8X16_HALF);
    }
    
    OLED_Update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 映射层菜单操作函数
 * 用于显示和切换当前的映射层
 */
// 重新设计的显示函数 - 在一个页面内显示所有必要信息
static void display_keymap_layer(int current_keymap_layer_val) {
    OLED_Clear();
    
    // 标题栏 - 居中显示
    OLED_ShowString(40, 0, "Keymap", OLED_6X8_HALF);
    
    // 当前映射层显示 - 大字体突出显示
    char layer_str[10];  // 增加数组大小防止溢出
    sprintf(layer_str, "Layer %d", current_keymap_layer_val);
    OLED_ShowString(30, 10, layer_str, OLED_8X16_HALF);
    
    OLED_Update();
}

/**
 * @brief 检查自定义图层(层1)是否为空
 * @return true 层1映射为空（所有按键都为0）
 * @return false 层1映射不为空（至少有一个按键非0）
 */
static bool is_layer1_empty(void) {
    // 检查层1是否有任何一个按键映射不为0
    for (int i = 0; i < NUM_KEYS; i++) {
        if (keymaps[1][i] != 0) {
            return false; // 层1不为空
        }
    }
    return true; // 层1为空
}

static void menuActionMappingLayer(void) {
    OLED_Clear();
    
    // 显示初始图层信息
    display_keymap_layer(current_keymap_layer);
    
    // 等待用户输入
    bool confirmed = false;
    while (!confirmed) {
        MenuOperation op;
        if (xQueueReceive(keyQueue, &op, 100 / portTICK_PERIOD_MS) == pdTRUE) {
            switch (op) {
                case MENU_OP_UP:
                    // 切换到图层0
                    current_keymap_layer = 0;
                    display_keymap_layer(current_keymap_layer);
                    break;
                case MENU_OP_DOWN:
                    // 检查层1是否为空
                    if (is_layer1_empty()) {
                        // 层1为空，显示提示信息
                        OLED_Clear();
                        OLED_ShowString(10, 10, "Layer 1 is empty!", OLED_8X16_HALF);
                        OLED_Update();
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        // 保持当前图层不变
                        display_keymap_layer(current_keymap_layer);
                    } else {
                        // 层1不为空，切换到图层1
                        current_keymap_layer = 1;
                        display_keymap_layer(current_keymap_layer);
                    }
                    break;
                case MENU_OP_ENTER:
                    // 保存选择并退出
                    OLED_Clear();
                    OLED_ShowString(35, 10, "Saved!", OLED_8X16_HALF);
                    OLED_Update();
                    
                    // 保存当前映射层选择到NVS
                    save_menu_config();
                    
                    vTaskDelay(500 / portTICK_PERIOD_MS); // 缩短等待时间
                    confirmed = true;
                    break;
                case MENU_OP_BACK:
                    // 不保存退出
                    confirmed = true;
                    break;
                default:
                    break;
            }
        }
    }
    
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}


// 定义菜单项索引枚举，使菜单层次关系更加直观
typedef enum {
    MENU_ID_MAIN ,              // 根菜单
    
    // 一级菜单 (根菜单的子项)
    MENU_ID_SYS_SETTINGS,          // 系统设置
    MENU_ID_KEYBOARD_OPTIONS,       // 键盘选项
    MENU_ID_NETWORK_CONFIG,        // 网络配置
    MENU_ID_REBOOT_DEVICE,         // 重启设备
    
    // 二级菜单 - 系统设置的子项
    MENU_ID_TIME_SETTINGS,         // 时间设置
    MENU_ID_POWER_OPTIONS,         // 电源选项
    
    // 二级菜单 - 键盘选项的子项
    MENU_ID_MAPPING_LAYER,         // 映射层
    MENU_ID_FACTORY_RESET,         // 恢复出厂设置
    
    // 二级菜单 - 键盘选项的子项
    MENU_ID_BRIGHTNESS,            // 亮度
    MENU_ID_CONTRAST,              // 对比度
    MENU_ID_TEST_DISPLAY,          // 测试显示
    
    // 三级菜单 - 时间设置的子项
    MENU_ID_SET_CURRENT_TIME,      // 设置当前时间
    MENU_ID_SET_TIME_FORMAT,       // 设置时间格式
    
    // 三级菜单 - 亮度的子项
    MENU_ID_ADJUST_BRIGHTNESS,     // 设置亮度
    MENU_ID_IP_DISPLAY,            // IP地址显示
    
    // WiFi相关菜单项
    MENU_ID_WIFI_TOGGLE,           // WiFi开关
    MENU_ID_WIFI_INFO,             // WiFi信息
    MENU_ID_HTML_URL,              // HTML网址
    MENU_ID_CLEAR_WIFI_PASSWORD    // 清除WiFi密码

} MenuItemId;

// 菜单定义结构
MenuItemDef menuItems[] = {
    // 根菜单
    {"Main Menu", MENU_TYPE_TEXT, NULL, 0, 0, NULL, -1},
    
    // 一级菜单 (父菜单为根菜单)
    {"系统设置", MENU_TYPE_IMAGE, Image_setings, 30, 30, NULL, MENU_ID_MAIN},
    {"键盘选项", MENU_TYPE_IMAGE, Image_keyboard, 30, 30, NULL, MENU_ID_MAIN},
    {"网络配置", MENU_TYPE_IMAGE, Image_wifi, 30, 30, NULL, MENU_ID_MAIN},
    {"重启设备", MENU_TYPE_IMAGE, Image_4, 32, 32, menuAction4, MENU_ID_MAIN},
    
    // 二级菜单 - 系统设置的子项
    {"时间设置", MENU_TYPE_TEXT, NULL, 0, 0, NULL, MENU_ID_SYS_SETTINGS},
    {"电源选项", MENU_TYPE_TEXT, NULL, 0, 0, NULL, MENU_ID_SYS_SETTINGS},
    {"恢复出厂设置", MENU_TYPE_ACTION, NULL, 0, 0, menuAction2, MENU_ID_SYS_SETTINGS},
    
    // 二级菜单 - 显示选项的子项
    {"映射层", MENU_TYPE_ACTION, NULL, 0, 0, menuActionMappingLayer, MENU_ID_KEYBOARD_OPTIONS},
    {"对比度", MENU_TYPE_TEXT, NULL, 0, 0, NULL, MENU_ID_KEYBOARD_OPTIONS},
    {"测试显示", MENU_TYPE_ACTION, NULL, 0, 0, menuAction3, MENU_ID_KEYBOARD_OPTIONS},
    
    // 三级菜单 - 时间设置的子项
    {"设置当前时间", MENU_TYPE_ACTION, NULL, 0, 0, menuAction1, MENU_ID_TIME_SETTINGS},
    {"设置时间格式", MENU_TYPE_ACTION, NULL, 0, 0, menuAction2, MENU_ID_TIME_SETTINGS},
    
    // 三级菜单 - 亮度的子项
    {"设置亮度", MENU_TYPE_ACTION, NULL, 0, 0, menuAction2, MENU_ID_TEST_DISPLAY},
    {"IP显示", MENU_TYPE_ACTION, NULL, 0, 0, menuAction3, MENU_ID_TEST_DISPLAY},
    
    // WiFi相关菜单项 - 网络配置的子项
    {"WiFi开关", MENU_TYPE_ACTION, NULL, 0, 0, menuActionWifiToggle, MENU_ID_NETWORK_CONFIG},
    {"WiFi信息", MENU_TYPE_ACTION, NULL, 0, 0, menuActionWifiStatus, MENU_ID_NETWORK_CONFIG},
    {"配置页面", MENU_TYPE_ACTION, NULL, 0, 0, menuActionHtmlUrl, MENU_ID_NETWORK_CONFIG},
    {"清除密码", MENU_TYPE_ACTION, NULL, 0, 0, menuActionClearWifiPassword, MENU_ID_NETWORK_CONFIG},
};

// 计算菜单项数量
const uint8_t MENU_ITEM_COUNT = sizeof(menuItems)/sizeof(MenuItemDef);

/**
 * @brief 初始化菜单结构
 */
static void menu_init(uint8_t fontSize) {
    MenuManager_Init(&menuManager);
    
    // 自动构建菜单树
    MenuItem* root = build_menu_tree();
    
    // 设置根菜单
    MenuManager_SetRootMenu(&menuManager, root);

    // 显示初始菜单
    MenuManager_DisplayMenu(&menuManager, 0, 0, fontSize);
}

/**
 * @brief 摇杆扫描任务 - 检测摇杆方向和按键状态
 */
static void joystick_task(void *arg) {

    sw_gpio_init();
    
    const TickType_t scanInterval = 10 / portTICK_PERIOD_MS;  // 100ms扫描周期
    joystick_state_t lastState = {JOYSTICK_CENTER, BUTTON_NONE};
    
    while (1) {
        // 获取摇杆状态
        joystick_state_t currentState = get_joystick_direction();
        
        // 根据摇杆方向发送菜单操作命令
        if (currentState.direction != lastState.direction) {
            switch (currentState.direction) {
                case JOYSTICK_UP:
                    xQueueSend(keyQueue, &(uint8_t){MENU_OP_UP}, 0);
                    break;
                case JOYSTICK_DOWN:
                    xQueueSend(keyQueue, &(uint8_t){MENU_OP_DOWN}, 0);
                    break;
                case JOYSTICK_LEFT:
                    xQueueSend(keyQueue, &(uint8_t){MENU_OP_LEFT}, 0); 
                    break;
                case JOYSTICK_RIGHT:
                    xQueueSend(keyQueue, &(uint8_t){MENU_OP_RIGHT}, 0);
                    break;
                default:
                    break;
            }
        }
        
        // 检测摇杆按键按下（短按）
        if (lastState.press_type != BUTTON_SHORT_PRESS && currentState.press_type == BUTTON_SHORT_PRESS) {
            xQueueSend(keyQueue, &(uint8_t){MENU_OP_ENTER}, 0);
        }
        
        // 检测摇杆按键长按
        if (lastState.press_type != BUTTON_LONG_PRESS && currentState.press_type == BUTTON_LONG_PRESS) {
            // TODO: 添加长按事件处理逻辑
            xQueueSend(keyQueue, &(uint8_t){MENU_OP_BACK}, 0);
        }
        
        // 检测摇杆按键双击
        if (lastState.press_type != BUTTON_DOUBLE_PRESS && currentState.press_type == BUTTON_DOUBLE_PRESS) {
            // TODO: 添加双击事件处理逻辑
            xQueueSend(keyQueue, &(uint8_t){MENU_OP_BACK}, 0);
        }
        
        // 保存当前状态
        lastState = currentState;
        
        vTaskDelay(scanInterval);
    }
}

/**
 * @brief 菜单显示任务
 */
static void menu_task(void *arg) {
    // 初始化OLED显示
    OLED_Init();    
    
    // 加载菜单配置
    load_menu_config();
    
    // 初始化菜单
    menu_init(OLED_8X16_HALF);
    
    // 菜单处理循环
    uint8_t keyCode;
    while (1) {
        // 等待按键事件
        if (xQueueReceive(keyQueue, &keyCode, portMAX_DELAY) == pdTRUE) {
            // 检查是否应该阻塞按键事件
            if (!menuManager.blockKeyEvents) {
                // 处理菜单操作
                if (MenuManager_HandleOperation(&menuManager, keyCode)) {
                    // 操作成功，刷新显示
                    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
                }
            }
        }
    }
}

/**
 * @brief 菜单系统入口函数
 */
void oled_menu_example_start(void) {
    // 创建操作队列
    keyQueue = xQueueCreate(10, sizeof(uint8_t));
    
    // 创建摇杆扫描任务
    xTaskCreate(joystick_task, "joystick_task", 3*1024, NULL, 5, NULL);
    
    // 创建菜单显示任务
    xTaskCreate(menu_task, "menu_task", 4096, NULL, 4, NULL);
}

/**
 * @brief 清空按键事件队列
 * 
 * 移除队列中的所有按键事件
 */
void MenuManager_ClearKeyQueue(void) {
    if (keyQueue != NULL) {
        xQueueReset(keyQueue);
    }
}
