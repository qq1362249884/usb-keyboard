#include "oled_menu_display.h"
#include "OLED.h"


// 内部static函数声明
static esp_err_t menu_nvs_init(void);
static void load_menu_config(void);
static void save_menu_config(void);
static void menu_init(uint8_t fontSize);
static void joystick_task(void *arg);
static void menu_task(void *arg);

// 灯效菜单操作函数声明
static void menuActionRgbToggle(void);
static void menuActionRgbModeSelect(void);
static void menuActionRgbSpeedAdjust(void);
static void menuActionRgbHueAdjust(void);
static void menuActionRgbSatAdjust(void);
static void menuActionRgbValAdjust(void);

// 定义菜单项索引枚举，使菜单层次关系更加直观
// 注意：新增菜单项时，请严格按照"根菜单→一级菜单→二级菜单→三级菜单"的顺序添加
//       同一层级内的菜单项应按照功能分组，保持与menuItems数组中的顺序完全一致
typedef enum {
    // 根菜单
    MENU_ID_MAIN,              
    
    // 一级菜单 (根菜单的子项)
    MENU_ID_SYS_SETTINGS,          // 系统设置
    MENU_ID_KEYBOARD_OPTIONS,      // 键盘选项
    MENU_ID_NETWORK_CONFIG,        // 网络配置
    MENU_ID_REBOOT_DEVICE,         // 重启设备
    
    // 二级菜单 - 系统设置的子项
    MENU_ID_TIME_SETTINGS,         // 时间设置
    MENU_ID_POWER_OPTIONS,         // 电源选项
    MENU_ID_FACTORY_RESET,         // 恢复出厂设置
    
    // 二级菜单 - 键盘选项的子项
    MENU_ID_MAPPING_LAYER,         // 映射层
    MENU_ID_CONTRAST,              // 对比度
    MENU_ID_TEST_DISPLAY,          // 测试显示
    MENU_ID_RGB_EFFECTS,           // 灯效管理
    
    // 二级菜单 - 网络配置的子项
    MENU_ID_WIFI_TOGGLE,           // WiFi开关
    MENU_ID_WIFI_INFO,             // WiFi信息
    MENU_ID_HTML_URL,              // HTML网址
    MENU_ID_CLEAR_WIFI_PASSWORD,   // 清除WiFi密码
    
    // 三级菜单 - 灯效管理的子项
    MENU_ID_RGB_TOGGLE,            // 开关灯效
    MENU_ID_RGB_MODE_SELECT,       // 选择灯效模式
    MENU_ID_RGB_SPEED_ADJUST,      // 调节灯效速度
    MENU_ID_RGB_HUE_ADJUST,        // 调节色调
    MENU_ID_RGB_SAT_ADJUST,        // 调节饱和度
    MENU_ID_RGB_VAL_ADJUST,        // 调节亮度
    
    // 三级菜单 - 时间设置的子项
    MENU_ID_SET_CURRENT_TIME,      // 设置当前时间
    MENU_ID_SET_TIME_FORMAT,       // 设置时间格式
    
    // 三级菜单 - 测试显示的子项
    MENU_ID_ADJUST_BRIGHTNESS,     // 设置亮度
    MENU_ID_IP_DISPLAY             // IP地址显示

} MenuItemId;

// 全局变量定义

// 按键状态队列
static QueueHandle_t keyQueue = NULL;

// 菜单管理器实例
static MenuManager menuManager;

// 全局变量用于存储当前映射层
uint8_t current_keymap_layer = 0; // 默认使用层0

// 全局菜单NVS管理器句柄
static MenuNvsManager_t* g_menu_nvs_manager = NULL;

// NVS相关函数实现

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

// 菜单操作函数实现

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

// 灯效菜单操作函数实现

/**
 * @brief 切换灯效开关
 */
static void menuActionRgbToggle(void) {
    OLED_Clear();
    if(kob_ws2812_is_enable() == false){
        kob_ws2812_enable(true);
        OLED_ShowString(20, 8, "RGB Enabled", OLED_8X16_HALF);
    }else{
        kob_ws2812_enable(false);
        OLED_ShowString(16, 8, "RGB Disabled", OLED_8X16_HALF);
    }
    OLED_Update();
    
    // 保存WS2812状态到NVS
    save_menu_config();
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 选择灯效模式
 */
static void menuActionRgbModeSelect(void) {
    OLED_Clear();
    
    // 获取当前灯效模式
    led_effect_config_t* config = kob_rgb_get_config();
    
    // 显示当前模式
    char mode_str[30];
    sprintf(mode_str, "Mode: %d", config->mode);
    OLED_ShowString(10, 4, mode_str, OLED_6X8_HALF);
    OLED_ShowString(10, 16, "Up/Down: Change", OLED_6X8_HALF);
    OLED_ShowString(10, 24, "Enter: Save", OLED_6X8_HALF);
    OLED_Update();
    
    // 等待用户输入
    bool confirmed = false;
    while (!confirmed) {
        MenuOperation op;
        if (xQueueReceive(keyQueue, &op, 100 / portTICK_PERIOD_MS) == pdTRUE) {
            switch (op) {
                case MENU_OP_UP:
                    kob_rgb_matrix_prev_mode();
                    break;
                case MENU_OP_DOWN:
                    kob_rgb_matrix_next_mode();
                    break;
                case MENU_OP_ENTER:
                    // 保存选择并退出
                    OLED_Clear();
                    OLED_ShowString(44, 4, "Saved!", OLED_8X16_HALF);
                    OLED_Update();
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    confirmed = true;
                    break;
                case MENU_OP_BACK:
                    // 不保存退出
                    confirmed = true;
                    break;
                default:
                    break;
            }
            
            // 更新显示
            if (op == MENU_OP_UP || op == MENU_OP_DOWN) {
                config = kob_rgb_get_config();
                sprintf(mode_str, "Mode: %d", config->mode);
                OLED_Clear();
                OLED_ShowString(10, 4, mode_str, OLED_6X8_HALF);
                OLED_ShowString(10, 16, "Up/Down: Change", OLED_6X8_HALF);
                OLED_ShowString(10, 24, "Enter: Save", OLED_6X8_HALF);
                OLED_Update();
            }
        }
    }
    
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 调节灯效速度
 */
static void menuActionRgbSpeedAdjust(void) {
    OLED_Clear();
    
    // 获取当前速度值
    led_effect_config_t* config = kob_rgb_get_config();
    
    // 显示当前速度
    char speed_str[30];
    sprintf(speed_str, "Speed: %d", config->speed);
    OLED_ShowString(10, 4, speed_str, OLED_6X8_HALF);
    OLED_ShowString(10, 16, "Up: Increase", OLED_6X8_HALF);
    OLED_ShowString(10, 24, "Down: Decrease", OLED_6X8_HALF);
    OLED_Update();
    
    // 等待用户输入
    bool confirmed = false;
    while (!confirmed) {
        MenuOperation op;
        if (xQueueReceive(keyQueue, &op, 100 / portTICK_PERIOD_MS) == pdTRUE) {
            switch (op) {
                case MENU_OP_UP:
                    kob_rgb_matrix_increase_speed();
                    break;
                case MENU_OP_DOWN:
                    kob_rgb_matrix_decrease_speed();
                    break;
                case MENU_OP_ENTER:
                    // 保存选择并退出
                    OLED_Clear();
                    OLED_ShowString(44, 4, "Saved!", OLED_6X8_HALF);
                    OLED_Update();
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    confirmed = true;
                    break;
                case MENU_OP_BACK:
                    // 不保存退出
                    confirmed = true;
                    break;
                default:
                    break;
            }
            
            // 更新显示
            if (op == MENU_OP_UP || op == MENU_OP_DOWN) {
                config = kob_rgb_get_config();
                sprintf(speed_str, "Speed: %d", config->speed);
                OLED_Clear();
                OLED_ShowString(10, 4, speed_str, OLED_6X8_HALF);
                OLED_ShowString(10, 16, "Up: Increase", OLED_6X8_HALF);
                OLED_ShowString(10, 24, "Down: Decrease", OLED_6X8_HALF);
                OLED_Update();
            }
        }
    }
    
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 调节色调
 */
static void menuActionRgbHueAdjust(void) {
    OLED_Clear();
    
    // 获取当前色调值
    led_effect_config_t* config = kob_rgb_get_config();
    
    // 显示当前色调
    char hue_str[30];
    sprintf(hue_str, "Hue: %d", config->hue);
    OLED_ShowString(10, 4, hue_str, OLED_6X8_HALF);
    OLED_ShowString(10, 16, "Up: Increase", OLED_6X8_HALF);
    OLED_ShowString(10, 24, "Down: Decrease", OLED_6X8_HALF);
    OLED_Update();
    
    // 等待用户输入
    bool confirmed = false;
    while (!confirmed) {
        MenuOperation op;
        if (xQueueReceive(keyQueue, &op, 100 / portTICK_PERIOD_MS) == pdTRUE) {
            switch (op) {
                case MENU_OP_UP:
                    kob_rgb_matrix_increase_hue();
                    break;
                case MENU_OP_DOWN:
                    kob_rgb_matrix_decrease_hue();
                    break;
                case MENU_OP_ENTER:
                    // 保存选择并退出
                    OLED_Clear();
                    OLED_ShowString(44, 4, "Saved!", OLED_8X16_HALF);
                    OLED_Update();
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    confirmed = true;
                    break;
                case MENU_OP_BACK:
                    // 不保存退出
                    confirmed = true;
                    break;
                default:
                    break;
            }
            
            // 更新显示
            if (op == MENU_OP_UP || op == MENU_OP_DOWN) {
                config = kob_rgb_get_config();
                sprintf(hue_str, "Hue: %d", config->hue);
                OLED_Clear();
                OLED_ShowString(10, 4, hue_str, OLED_6X8_HALF);
                OLED_ShowString(10, 16, "Up: Increase", OLED_6X8_HALF);
                OLED_ShowString(10, 24, "Down: Decrease", OLED_6X8_HALF);
                OLED_Update();
            }
        }
    }
    
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 调节饱和度
 */
static void menuActionRgbSatAdjust(void) {
    OLED_Clear();
    
    // 获取当前饱和度值
    led_effect_config_t* config = kob_rgb_get_config();
    
    // 显示当前饱和度
    char sat_str[30];
    sprintf(sat_str, "Saturation: %d", config->sat);
    OLED_ShowString(10, 4, sat_str, OLED_6X8_HALF);
    OLED_ShowString(10, 16, "Up: Increase", OLED_6X8_HALF);
    OLED_ShowString(10, 24, "Down: Decrease", OLED_6X8_HALF);
    OLED_Update();
    
    // 等待用户输入
    bool confirmed = false;
    while (!confirmed) {
        MenuOperation op;
        if (xQueueReceive(keyQueue, &op, 100 / portTICK_PERIOD_MS) == pdTRUE) {
            switch (op) {
                case MENU_OP_UP:
                    kob_rgb_matrix_increase_sat();
                    break;
                case MENU_OP_DOWN:
                    kob_rgb_matrix_decrease_sat();
                    break;
                case MENU_OP_ENTER:
                    // 保存选择并退出
                    OLED_Clear();
                    OLED_ShowString(44, 4, "Saved!", OLED_8X16_HALF);
                    OLED_Update();
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    confirmed = true;
                    break;
                case MENU_OP_BACK:
                    // 不保存退出
                    confirmed = true;
                    break;
                default:
                    break;
            }
            
            // 更新显示
            if (op == MENU_OP_UP || op == MENU_OP_DOWN) {
                config = kob_rgb_get_config();
                sprintf(sat_str, "Saturation: %d", config->sat);
                OLED_Clear();
                OLED_ShowString(10, 4, sat_str, OLED_6X8_HALF);
                OLED_ShowString(10, 16, "Up: Increase", OLED_6X8_HALF);
                OLED_ShowString(10, 24, "Down: Decrease", OLED_6X8_HALF);
                OLED_Update();
            }
        }
    }
    
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 调节亮度
 */
static void menuActionRgbValAdjust(void) {
    OLED_Clear();
    
    // 获取当前亮度值
    led_effect_config_t* config = kob_rgb_get_config();
    
    // 显示当前亮度
    char val_str[30];
    sprintf(val_str, "Brightness: %d", config->val);
    OLED_ShowString(10, 4, val_str, OLED_6X8_HALF);
    OLED_ShowString(10, 16, "Up: Increase", OLED_6X8_HALF);
    OLED_ShowString(10, 24, "Down: Decrease", OLED_6X8_HALF);
    OLED_Update();
    
    // 等待用户输入
    bool confirmed = false;
    while (!confirmed) {
        MenuOperation op;
        if (xQueueReceive(keyQueue, &op, 100 / portTICK_PERIOD_MS) == pdTRUE) {
            switch (op) {
                case MENU_OP_UP:
                    kob_rgb_matrix_increase_val();
                    break;
                case MENU_OP_DOWN:
                    kob_rgb_matrix_decrease_val();
                    break;
                case MENU_OP_ENTER:
                    // 保存选择并退出
                    OLED_Clear();
                    OLED_ShowString(44, 4, "Saved!", OLED_8X16_HALF);
                    OLED_Update();
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    confirmed = true;
                    break;
                case MENU_OP_BACK:
                    // 不保存退出
                    confirmed = true;
                    break;
                default:
                    break;
            }
            
            // 更新显示
            if (op == MENU_OP_UP || op == MENU_OP_DOWN) {
                config = kob_rgb_get_config();
                sprintf(val_str, "Brightness: %d", config->val);
                OLED_Clear();
                OLED_ShowString(10, 4, val_str, OLED_6X8_HALF);
                OLED_ShowString(10, 16, "Up: Increase", OLED_6X8_HALF);
                OLED_ShowString(10, 24, "Down: Decrease", OLED_6X8_HALF);
                OLED_Update();
            }
        }
    }
    
    MenuManager_DisplayMenu(&menuManager, 0, 0, OLED_8X16_HALF);
}

// 菜单定义结构
MenuItemDef menuItems[] = {
    // 根菜单
    {"Main Menu", MENU_TYPE_IMAGE, Image_setings, 32, 32, NULL, -1},
    
    // 一级菜单 (父菜单为根菜单)
    {"系统设置", MENU_TYPE_IMAGE, Image_setings, 30, 30, NULL, MENU_ID_MAIN},
    {"键盘选项", MENU_TYPE_IMAGE, Image_keyboard, 30, 30, NULL, MENU_ID_MAIN},
    {"网络配置", MENU_TYPE_IMAGE, Image_wifi, 30, 30, NULL, MENU_ID_MAIN},
    {"重启设备", MENU_TYPE_IMAGE, Image_4, 32, 32, menuAction4, MENU_ID_MAIN},
    
    // 二级菜单 - 系统设置的子项
    {"时间设置", MENU_TYPE_TEXT, NULL, 0, 0, NULL, MENU_ID_SYS_SETTINGS},
    {"电源选项", MENU_TYPE_TEXT, NULL, 0, 0, NULL, MENU_ID_SYS_SETTINGS},
    {"恢复出厂设置", MENU_TYPE_ACTION, NULL, 0, 0, menuAction2, MENU_ID_SYS_SETTINGS},
    
    // 二级菜单 - 键盘选项的子项
    {"映射层", MENU_TYPE_ACTION, NULL, 0, 0, menuActionMappingLayer, MENU_ID_KEYBOARD_OPTIONS},
    {"对比度", MENU_TYPE_TEXT, NULL, 0, 0, NULL, MENU_ID_KEYBOARD_OPTIONS},
    {"测试显示", MENU_TYPE_TEXT, NULL, 0, 0, NULL, MENU_ID_KEYBOARD_OPTIONS},
    {"RGB Effects", MENU_TYPE_TEXT, NULL, 0, 0, NULL, MENU_ID_KEYBOARD_OPTIONS},
    
    // 三级菜单 - 灯效管理的子项
    {"Toggle RGB", MENU_TYPE_ACTION, NULL, 0, 0, menuActionRgbToggle, MENU_ID_RGB_EFFECTS},
    {"Effect Mode", MENU_TYPE_ACTION, NULL, 0, 0, menuActionRgbModeSelect, MENU_ID_RGB_EFFECTS},
    {"Speed", MENU_TYPE_ACTION, NULL, 0, 0, menuActionRgbSpeedAdjust, MENU_ID_RGB_EFFECTS},
    {"Hue", MENU_TYPE_ACTION, NULL, 0, 0, menuActionRgbHueAdjust, MENU_ID_RGB_EFFECTS},
    {"Saturation", MENU_TYPE_ACTION, NULL, 0, 0, menuActionRgbSatAdjust, MENU_ID_RGB_EFFECTS},
    {"Brightness", MENU_TYPE_ACTION, NULL, 0, 0, menuActionRgbValAdjust, MENU_ID_RGB_EFFECTS},
    
    // 三级菜单 - 时间设置的子项
    {"设置当前时间", MENU_TYPE_ACTION, NULL, 0, 0, menuAction1, MENU_ID_TIME_SETTINGS},
    {"设置时间格式", MENU_TYPE_ACTION, NULL, 0, 0, menuAction2, MENU_ID_TIME_SETTINGS},
    
    // 三级菜单 - 测试显示的子项
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

// 菜单初始化函数

/**
 * @brief 初始化菜单结构
 */
static void menu_init(uint8_t fontSize) {
    MenuManager_Init(&menuManager);
    
    // 验证MenuItemId枚举与menuItems数组的一致性
    // 确保MenuItemId枚举中最大的值不超过menuItems数组的大小
    if (MENU_ID_IP_DISPLAY >= MENU_ITEM_COUNT) {
        ESP_LOGE("OLED_MENU", "MenuItemId枚举数量与menuItems数组数量不一致! 请检查MenuItemId枚举定义.");
    }
    
    // 自动构建菜单树
    MenuItem* root = build_menu_tree();
    
    // 设置根菜单
    MenuManager_SetRootMenu(&menuManager, root);

    // 显示初始菜单
    MenuManager_DisplayMenu(&menuManager, 0, 0, fontSize);
}

// 任务函数

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

// 公共API函数

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

// 公共访问函数定义
QueueHandle_t get_key_queue(void) {
    return keyQueue;
}

MenuManager* get_menu_manager(void) {
    return &menuManager;
}
