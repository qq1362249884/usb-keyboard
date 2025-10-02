#include "oled_menu_display.h"
#include "spi_scanner/keymap_manager.h"
#include "nvs_manager/unified_nvs_manager.h"

// 内部static函数声明
static esp_err_t menu_nvs_init(void);
static void load_menu_config(void);
static void save_menu_config(void);
static void menu_init(uint8_t fontSize);
static void joystick_task(void *arg);
static void menu_task(void *arg);




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
    MENU_ID_CALCULATOR,
    
    // 二级菜单 - 系统设置的子项
    MENU_ID_TIME_SETTINGS,         // 时间设置
    
    // 二级菜单 - 键盘选项的子项
    MENU_ID_MAPPING_LAYER,         // 映射层
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
    MENU_ID_RGB_HSV_ADJUST         // HSV统一调控

} MenuItemId;

// 全局变量定义

// 摇杆操作码队列（uint8_t类型）
static QueueHandle_t joystickQueue = NULL;

// 键盘键码队列（uint16_t类型）
static QueueHandle_t keyboardQueue = NULL;

// 菜单管理器实例
static MenuManager menuManager;

// 全局变量用于存储当前映射层
uint8_t current_keymap_layer = 0; // 默认使用层0

// 使用全局统一NVS管理器句柄
extern unified_nvs_manager_t *g_unified_nvs_manager;

/**
 * @brief 设置统一NVS管理器句柄
 * @param manager 外部NVS管理器实例
 */
void set_unified_nvs_manager(unified_nvs_manager_t* manager) {
    g_unified_nvs_manager = manager;
}

/**
 * @brief 获取统一NVS管理器句柄
 * @return 统一NVS管理器句柄，若未初始化则返回NULL
 */
unified_nvs_manager_t* get_unified_nvs_manager(void) {
    return g_unified_nvs_manager;
}

// NVS相关函数实现

/**
 * @brief 初始化菜单NVS管理器
 * @return ESP_OK 成功
 * @return 其他 失败
 */
static esp_err_t menu_nvs_init(void) {
    if (g_unified_nvs_manager) {
        return ESP_OK;
    }
    
    // 使用全局NVS管理器，不再创建自己的实例
    ESP_LOGW("MENU_NVS", "Global NVS manager not available, menu NVS functions will be disabled");
    return ESP_ERR_NOT_SUPPORTED;
}

/**
 * @brief 从NVS加载菜单配置
 */
static void load_menu_config(void) {
    if (!g_unified_nvs_manager) {
        if (menu_nvs_init() != ESP_OK) {
            ESP_LOGE("MENU_NVS", "Failed to initialize NVS manager during config load");
            return;
        }
    }
    
    // 记录加载前的状态
    uint8_t prev_layer = current_keymap_layer;
    bool prev_ws2812_state = kob_ws2812_is_enable();
    
    ESP_LOGI("MENU_NVS", "Before loading config: layer=%d, ws2812_state=%d", 
             prev_layer, prev_ws2812_state);
    
    // 准备加载的变量
    uint8_t layer = current_keymap_layer;
    bool ws2812_state = prev_ws2812_state;
    
    // 从菜单配置读取层和WS2812状态
    esp_err_t menu_err = unified_nvs_load_menu_config(g_unified_nvs_manager, &layer, &ws2812_state);
    if (menu_err != ESP_OK && menu_err != ESP_ERR_NOT_FOUND) {
        ESP_LOGE("MENU_NVS", "Failed to load menu config: %s", esp_err_to_name(menu_err));
        return;
    }
    
    // 无论加载结果如何，都应用合理的值
    if (menu_err == ESP_OK || menu_err == ESP_ERR_NOT_FOUND) {
        // 确保加载的层值在有效范围内
        if (layer < TOTAL_LAYERS) {
            current_keymap_layer = layer;
        } else {
            ESP_LOGW("MENU_NVS", "Loaded layer %d is out of range, keeping current layer %d", 
                     layer, current_keymap_layer);
        }
        
        // 强制设置WS2812状态 - 关键修改点
        ESP_LOGI("MENU_NVS", "Applying WS2812 state from NVS: %d", ws2812_state);
        
        // 添加更长的延时确保LED驱动完全初始化完成
        vTaskDelay(200 / portTICK_PERIOD_MS);
        
        // 应用WS2812状态，并添加更可靠的重试机制
        bool state_applied = false;
        int retry_count = 0;
        const int max_retries = 5; // 增加重试次数
        
        while (!state_applied && retry_count < max_retries) {
            kob_ws2812_enable(ws2812_state);
            
            // 增加延时让状态完全生效
            vTaskDelay(50 / portTICK_PERIOD_MS);
            
            // 再次检查状态是否正确应用
            bool applied_state = kob_ws2812_is_enable();
            ESP_LOGI("MENU_NVS", "WS2812 state after application (attempt %d): %d", 
                     retry_count + 1, applied_state);
            
            if (applied_state == ws2812_state) {
                state_applied = true;
                ESP_LOGI("MENU_NVS", "WS2812 state successfully applied and verified");
            } else {
                ESP_LOGW("MENU_NVS", "WS2812 state mismatch after application! Expected: %d, Actual: %d", 
                         ws2812_state, applied_state);
                retry_count++;
                vTaskDelay(50 / portTICK_PERIOD_MS); // 增加再次尝试前的延时
            }
        }
        
        if (!state_applied) {
            ESP_LOGE("MENU_NVS", "Failed to apply WS2812 state after %d attempts", max_retries);
            // 即使失败也强制设置状态，避免使用初始值
            kob_ws2812_enable(ws2812_state);
            ESP_LOGW("MENU_NVS", "Forcibly set WS2812 state to: %d", ws2812_state);
        }
        
    }
    
    // 记录加载后的状态
    ESP_LOGI("MENU_NVS", "After loading config: layer=%d, ws2812_state=%d", 
             current_keymap_layer, kob_ws2812_is_enable());
}

/**
 * @brief 保存菜单配置到NVS
 */
static void save_menu_config(void) {
    if (!g_unified_nvs_manager) {
        if (menu_nvs_init() != ESP_OK) {
            return;
        }
    }
    
    // 获取当前WS2812状态
    bool ws2812_state = kob_ws2812_is_enable();
    
    // WiFi状态由wifi_app模块统一管理，菜单系统不再保存WiFi状态
    // 使用统一的菜单配置保存函数
    unified_nvs_save_menu_config(g_unified_nvs_manager, current_keymap_layer, ws2812_state);
}


// 菜单定义结构
MenuItemDef menuItems[] = {
    // 根菜单
    {"Main Menu", MENU_TYPE_IMAGE, Image_setings, 32, 32, NULL, -1},
    
    // 一级菜单 (父菜单为根菜单)
    {"系统设置", MENU_TYPE_IMAGE, Image_setings, 30, 30, NULL, MENU_ID_MAIN},
    {"键盘选项", MENU_TYPE_IMAGE, Image_keyboard, 30, 30, NULL, MENU_ID_MAIN},
    {"网络配置", MENU_TYPE_IMAGE, Image_wifi, 30, 30, NULL, MENU_ID_MAIN},
    {"计算器", MENU_TYPE_IMAGE, Image_custom, 30, 30, menuActionCalculator, MENU_ID_MAIN},
    
    // 二级菜单 - 系统设置的子项
    {"时间设置", MENU_TYPE_TEXT, NULL, 0, 0, NULL, MENU_ID_SYS_SETTINGS},
    
    // 二级菜单 - 键盘选项的子项
    {"映射层", MENU_TYPE_ACTION, NULL, 0, 0, menuActionMappingLayer, MENU_ID_KEYBOARD_OPTIONS},
    {"灯效管理", MENU_TYPE_TEXT, NULL, 0, 0, NULL, MENU_ID_KEYBOARD_OPTIONS},
    
    // 三级菜单 - 灯效管理的子项
    {"开关灯效", MENU_TYPE_ACTION, NULL, 0, 0, menuActionRgbToggle, MENU_ID_RGB_EFFECTS},
    {"灯效模式", MENU_TYPE_ACTION, NULL, 0, 0, menuActionRgbModeSelect, MENU_ID_RGB_EFFECTS},
    {"速度", MENU_TYPE_ACTION, NULL, 0, 0, menuActionRgbSpeedAdjust, MENU_ID_RGB_EFFECTS},
    {"HSV", MENU_TYPE_ACTION, NULL, 0, 0, menuActionRgbHsvAdjust, MENU_ID_RGB_EFFECTS},
    
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
    if (MENU_ID_RGB_HSV_ADJUST >= MENU_ITEM_COUNT) {
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
    
    const TickType_t scanInterval = 10 / portTICK_PERIOD_MS;  // 10ms扫描周期
    joystick_state_t lastState = {JOYSTICK_CENTER, BUTTON_NONE};
    
    // 摇杆长拉相关变量
    typedef struct {
        bool active;              // 是否处于长拉状态
        uint32_t start_time;      // 长拉开始时间
        uint32_t repeat_count;    // 重复次数
        uint32_t next_repeat_time; // 下一次重复的时间
    } JoystickHoldState;
    
    JoystickHoldState holdState = {false, 0, 0, 0};
    
    // 长拉参数设置
    const uint32_t initial_delay = 300;  // 首次重复前的延迟时间(ms)
    const uint32_t fast_repeat_delay = 50; // 快速重复时间间隔(ms)
    const uint32_t slow_repeat_delay = 200; // 慢速重复时间间隔(ms)
    const uint32_t acceleration_threshold = 5; // 加速阈值（重复次数）
    
    while (1) {
        // 获取当前时间
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // 获取摇杆状态
        joystick_state_t currentState = get_joystick_direction();
        
        // 检查方向变化或长拉状态
        if (currentState.direction != JOYSTICK_CENTER) {
            // 方向不为中心
            if (currentState.direction != lastState.direction) {
                // 方向改变，发送初始事件
                switch (currentState.direction) {
                    case JOYSTICK_UP:
                        xQueueSend(joystickQueue, &(uint8_t){MENU_OP_UP}, 0);
                        break;
                    case JOYSTICK_DOWN:
                        xQueueSend(joystickQueue, &(uint8_t){MENU_OP_DOWN}, 0);
                        break;
                    case JOYSTICK_LEFT:
                        xQueueSend(joystickQueue, &(uint8_t){MENU_OP_LEFT}, 0); 
                        break;
                    case JOYSTICK_RIGHT:
                        xQueueSend(joystickQueue, &(uint8_t){MENU_OP_RIGHT}, 0);
                        break;
                    default:
                        break;
                }
                
                // 初始化长拉状态
                holdState.active = true;
                holdState.start_time = current_time;
                holdState.repeat_count = 0;
                holdState.next_repeat_time = current_time + initial_delay;
            } else if (holdState.active) {
                // 持续保持同一方向，检查是否需要发送重复事件
                if (current_time >= holdState.next_repeat_time) {
                    // 发送重复事件
                    switch (currentState.direction) {
                        case JOYSTICK_UP:
                            xQueueSend(joystickQueue, &(uint8_t){MENU_OP_UP}, 0);
                            break;
                        case JOYSTICK_DOWN:
                            xQueueSend(joystickQueue, &(uint8_t){MENU_OP_DOWN}, 0);
                            break;
                        case JOYSTICK_LEFT:
                            xQueueSend(joystickQueue, &(uint8_t){MENU_OP_LEFT}, 0); 
                            break;
                        case JOYSTICK_RIGHT:
                            xQueueSend(joystickQueue, &(uint8_t){MENU_OP_RIGHT}, 0);
                            break;
                        default:
                            break;
                    }
                    
                    // 更新重复计数和下一次重复时间
                    holdState.repeat_count++;
                    
                    // 根据重复次数调整重复速度（加速机制）
                    if (holdState.repeat_count >= acceleration_threshold) {
                        holdState.next_repeat_time = current_time + fast_repeat_delay;
                    } else {
                        holdState.next_repeat_time = current_time + slow_repeat_delay;
                    }
                }
            }
        } else {
            // 方向回到中心，立即重置所有长拉状态，确保不再发送重复事件
            holdState.active = false;
            holdState.repeat_count = 0;
            holdState.next_repeat_time = 0;
            
            // 清空事件队列，防止残留的重复事件被处理
            uint8_t dummy;
            while (uxQueueMessagesWaiting(joystickQueue) > 0) {
                xQueueReceive(joystickQueue, &dummy, 0);
            }
        }
        
        // 检测摇杆按键按下（短按）
        if (lastState.press_type != BUTTON_SHORT_PRESS && currentState.press_type == BUTTON_SHORT_PRESS) {
            xQueueSend(joystickQueue, &(uint8_t){MENU_OP_ENTER}, 0);
        }
        
        // 检测摇杆按键长按
        if (lastState.press_type != BUTTON_LONG_PRESS && currentState.press_type == BUTTON_LONG_PRESS) {
            // TODO: 添加长按事件处理逻辑
            xQueueSend(joystickQueue, &(uint8_t){MENU_OP_BACK}, 0);
        }
        
        // 检测摇杆按键双击
        if (lastState.press_type != BUTTON_DOUBLE_PRESS && currentState.press_type == BUTTON_DOUBLE_PRESS) {
            // TODO: 添加双击事件处理逻辑
            xQueueSend(joystickQueue, &(uint8_t){MENU_OP_BACK}, 0);
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
        if (xQueueReceive(joystickQueue, &keyCode, portMAX_DELAY) == pdTRUE) {
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
    // 创建摇杆操作码队列（uint8_t类型）
    joystickQueue = xQueueCreate(10, sizeof(uint8_t));
    
    // 创建键盘键码队列（uint16_t类型）
    keyboardQueue = xQueueCreate(20, sizeof(uint16_t));
    
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
    if (joystickQueue != NULL) {
        xQueueReset(joystickQueue);
    }
}

// 公共访问函数定义
QueueHandle_t get_keyboard_queue(void) {
    return keyboardQueue;
}

QueueHandle_t get_joystick_queue(void) {
    return joystickQueue;
}

MenuManager* get_menu_manager(void) {
    return &menuManager;
}
