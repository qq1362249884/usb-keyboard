/*
 * OLED菜单键盘相关功能实现
 * 提供键盘映射层显示、切换等功能
 */

// 标准库头文件
#include <stdbool.h>
#include <string.h>

// ESP-IDF组件头文件
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// 项目内部头文件
#include "oled_menu_combined.h"
#include "oled_menu_display.h"
#include "spi_scanner/keymap_manager.h" // 访问keymaps数组
#include "nvs_manager/unified_nvs_manager.h" // 统一NVS管理器
#include "keyboard_led/keyboard_led.h" // LED控制

// 全局变量声明
extern uint8_t current_keymap_layer; // 在oled_menu_display.c中定义

// 从oled_menu_display.c获取的函数声明
extern QueueHandle_t get_joystick_queue(void);
extern MenuManager* get_menu_manager(void);
extern unified_nvs_manager_t* get_unified_nvs_manager(void);

/**
 * @brief 显示当前映射层信息
 * @param current_keymap_layer_val 当前映射层值
 */
void display_keymap_layer(int current_keymap_layer_val) {
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
 * @brief 检查指定图层是否为空
 * @param layer 层索引
 * @return true 层映射为空（所有按键都为0）
 * @return false 层映射不为空（至少有一个按键非0）
 */
bool is_layer_empty(uint8_t layer) {
    // 检查指定层是否有任何一个按键映射不为0
    for (int i = 0; i < NUM_KEYS; i++) {
        if (keymaps[layer][i] != 0) {
            return false; // 层不为空
        }
    }
    return true; // 层为空
}

/**
 * @brief 映射层菜单操作函数
 * 用于显示和切换当前的映射层
 */
void menuActionMappingLayer(void) {
    OLED_Clear();
    
    // 显示初始图层信息
    display_keymap_layer(current_keymap_layer);
    
    // 等待用户输入
    bool confirmed = false;
    while (!confirmed) {
        uint8_t op; // 改为uint8_t类型，与joystick_task中发送的数据类型匹配
        if (xQueueReceive(get_joystick_queue(), &op, 100 / portTICK_PERIOD_MS) == pdTRUE) {
            switch (op) {
                case MENU_OP_UP:
                    // 向上切换到前一层（循环切换）
                    if (current_keymap_layer == 0) {
                        current_keymap_layer = TOTAL_LAYERS - 1; // 从层0切换到最后一层
                    } else {
                        current_keymap_layer--;
                    }
                    display_keymap_layer(current_keymap_layer);
                    break;
                case MENU_OP_DOWN:
                    // 向下切换到下一层（循环切换）
                    if (current_keymap_layer == TOTAL_LAYERS - 1) {
                        current_keymap_layer = 0; // 从最后一层切换到层0
                    } else {
                        current_keymap_layer++;
                    }
                    display_keymap_layer(current_keymap_layer);
                    break;
                case MENU_OP_ENTER:
                    // 检查选择的层是否为空，只有非空层才保存
                    if (!is_layer_empty(current_keymap_layer)) {
                        // 保存选择并退出
                        OLED_Clear();
                        // "Saved!" 居中显示：字符数6，字体宽度8px，总宽度48px，起始x=(128-48)/2=40
                        OLED_ShowString(40, 4, "Saved!", OLED_8X16_HALF);
                        OLED_Update();
                        
                        // 保存当前映射层选择到NVS
                        unified_nvs_manager_t* nvs_manager = get_unified_nvs_manager();
                        if (nvs_manager) {
                            // 只保存当前层信息，WS2812状态在灯效菜单中单独保存
                            unified_nvs_save_menu_config(nvs_manager, current_keymap_layer, false);
                        }
                        
                        // 保存当前层的键盘映射到NVS
                        esp_err_t save_err = save_keymap_to_nvs(current_keymap_layer, &keymaps[current_keymap_layer][0]);
                        if (save_err != ESP_OK) {
                            ESP_LOGE("OLED_MENU", "Failed to save keymap for layer %d", current_keymap_layer);
                        } else {
                            ESP_LOGI("OLED_MENU", "Successfully saved keymap for layer %d", current_keymap_layer);
                        }
                        
                        vTaskDelay(500 / portTICK_PERIOD_MS); // 缩短等待时间
                        confirmed = true;
                        
                    } else {
                        // 层为空，显示提示信息
                        OLED_Clear();
                        // "Layer Empty!" 使用OLED_6X8_HALF字体：字符数11，字体宽度6px，总宽度66px，起始x=(128-66)/2=31
                        OLED_ShowString(31, 8, "Layer Empty!", OLED_6X8_HALF);
                        // "No key mappings" 使用OLED_6X8_HALF字体：字符数14，字体宽度6px，总宽度84px，起始x=(128-84)/2=22
                        OLED_ShowString(22, 16, "No key mappings", OLED_6X8_HALF);
                        OLED_Update();
                        vTaskDelay(500 / portTICK_PERIOD_MS); // 缩短等待时间
                        display_keymap_layer(current_keymap_layer);
                    }
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
    
    MenuManager_DisplayMenu(get_menu_manager(), 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 切换灯效开关
 */
void menuActionRgbToggle(void) {
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
    unified_nvs_manager_t* nvs_manager = get_unified_nvs_manager();
    if (nvs_manager) {
        unified_nvs_save_menu_config(nvs_manager, current_keymap_layer, kob_ws2812_is_enable());
    }
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    MenuManager_DisplayMenu(get_menu_manager(), 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 选择灯效模式
 */
void menuActionRgbModeSelect(void) {
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
        uint8_t op; // 改为uint8_t类型，与joystick_task中发送的数据类型匹配
        if (xQueueReceive(get_joystick_queue(), &op, 100 / portTICK_PERIOD_MS) == pdTRUE) {
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
    
    MenuManager_DisplayMenu(get_menu_manager(), 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 调节灯效速度
 */
void menuActionRgbSpeedAdjust(void) {
    OLED_Clear();
    
    // 获取当前速度值
    led_effect_config_t* config = kob_rgb_get_config();
    
    // 从内部0-255转换为显示0-100%
    uint8_t display_speed = (config->speed * 100 + 127) / 255;  // 使用四舍五入
    
    // 显示当前速度百分比
    char speed_str[30];
    sprintf(speed_str, "Speed: %d%%", display_speed);
    OLED_ShowString(10, 4, speed_str, OLED_6X8_HALF);
    OLED_ShowString(10, 16, "Up: Increase 1%%", OLED_6X8_HALF);
    OLED_ShowString(10, 24, "Down: Decrease 1%%", OLED_6X8_HALF);
    OLED_Update();
    
    // 等待用户输入
    bool confirmed = false;
    while (!confirmed) {
        uint8_t op; // 改为uint8_t类型，与joystick_task中发送的数据类型匹配
        if (xQueueReceive(get_joystick_queue(), &op, 100 / portTICK_PERIOD_MS) == pdTRUE) {
            switch (op) {
                case MENU_OP_UP:
                    // 增加1%的速度
                    display_speed = (display_speed >= 100) ? 100 : (display_speed + 1);
                    // 转换回内部0-255值并设置
                    config->speed = (display_speed * 255) / 100;
                    kob_rgb_matrix_set_speed(config->speed);
                    break;
                case MENU_OP_DOWN:
                    // 减少1%的速度
                    display_speed = (display_speed == 0) ? 0 : (display_speed - 1);
                    // 转换回内部0-255值并设置
                    config->speed = (display_speed * 255) / 100;
                    kob_rgb_matrix_set_speed(config->speed);
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
                sprintf(speed_str, "Speed: %d%%", display_speed);
                OLED_Clear();
                OLED_ShowString(10, 4, speed_str, OLED_6X8_HALF);
                OLED_ShowString(10, 16, "Up: Increase 1%%", OLED_6X8_HALF);
                OLED_ShowString(10, 24, "Down: Decrease 1%%", OLED_6X8_HALF);
                OLED_Update();
            }
        }
    }
    
    MenuManager_DisplayMenu(get_menu_manager(), 0, 0, OLED_8X16_HALF);
}

/**
 * @brief HSV调控统一函数
 * 集成色调(0-360)、饱和度(0-100%)和亮度(0-100%)の调节
 */
void menuActionRgbHsvAdjust(void) {
    OLED_Clear();
    
    // 获取当前配置
    led_effect_config_t* config = kob_rgb_get_config();
    
    // 定义HSV调节状态
    typedef enum {
        HSV_ADJUST_HUE,
        HSV_ADJUST_SAT,
        HSV_ADJUST_VAL
    } HsvAdjustMode;
    
    HsvAdjustMode currentMode = HSV_ADJUST_HUE;
    char hsv_str[3][30];
    
    // 从内部值转换为标准显示值
    uint16_t display_hue = (config->hue * 360) / 255; // 内部0-255转为显示0-360
    uint8_t display_sat = (config->sat * 100 + 127) / 255;  // 内部0-255转为显示0-100%，使用四舍五入
    uint8_t display_val = (config->val * 100 + 127) / 255;  // 内部0-255转为显示0-100%，使用四舍五入
    
    // 使用简洁标签缩短文本长度，避免超出屏幕
    sprintf(hsv_str[0], "H: %d", display_hue);
    sprintf(hsv_str[1], "S: %d%%", display_sat);
    sprintf(hsv_str[2], "V: %d%%", display_val);
    
    // 考虑">"符号占用12个像素，设置适当的x坐标
    OLED_ShowString(12, 8, hsv_str[0], OLED_6X8_HALF);
    OLED_ShowString(12, 16, hsv_str[1], OLED_6X8_HALF);
    OLED_ShowString(12, 24, hsv_str[2], OLED_6X8_HALF);
    OLED_ShowString(0, 8 + currentMode * 8, " >", OLED_6X8_HALF); // 显示选中项标记
    
    OLED_Update();
    
    // 等待用户输入
    bool confirmed = false;
    while (!confirmed) {
        uint8_t op; // 直接使用uint8_t类型，与joystick_task中发送的数据类型匹配
        uint8_t h, s, v; // 在条件外声明变量，确保在所有使用场景中都可用
        if (xQueueReceive(get_joystick_queue(), &op, 100 / portTICK_PERIOD_MS) == pdTRUE) {
            // 使用固定步长1，由底层摇杆任务的长拉功能提供调节速度
            
            switch (op) {
                case MENU_OP_UP:
                    // 向上切换选中项
                    currentMode = (currentMode > 0) ? (currentMode - 1) : HSV_ADJUST_VAL;
                    break;
                case MENU_OP_DOWN:
                    // 向下切换选中项
                    currentMode = (currentMode < HSV_ADJUST_VAL) ? (currentMode + 1) : HSV_ADJUST_HUE;
                    break;
                case MENU_OP_LEFT:
                    // 减少选中参数的值
                    switch (currentMode) {
                        case HSV_ADJUST_HUE:
                            // 处理显示值的调整，使用固定步长1
                            display_hue = (display_hue > 1) ? (display_hue - 1) : (360 - (1 - display_hue - 1)); // 0-360度循环
                            // 转换回内部值并设置
                            h = (display_hue * 255) / 360;
                            kob_rgb_matrix_set_hsv(h, config->sat, config->val);
                            // 重新获取配置以确保一致性
                            config = kob_rgb_get_config();
                            break;
                        case HSV_ADJUST_SAT:
                            // 处理显示值的调整，使用固定步长1
                            display_sat = (display_sat > 1) ? (display_sat - 1) : 0; // 0-100%
                            // 转换回内部值并设置
                            s = (display_sat * 255) / 100;
                            kob_rgb_matrix_set_hsv(config->hue, s, config->val);
                            // 重新获取配置以确保一致性
                            config = kob_rgb_get_config();
                            break;
                        case HSV_ADJUST_VAL:
                            // 处理显示值的调整，使用固定步长1
                            display_val = (display_val > 1) ? (display_val - 1) : 0; // 0-100%
                            // 转换回内部值并设置
                            v = (display_val * 255) / 100;
                            kob_rgb_matrix_set_hsv(config->hue, config->sat, v);
                            // 重新获取配置以确保一致性
                            config = kob_rgb_get_config();
                            break;
                    }
                    break;
                case MENU_OP_RIGHT:
                    // 增加选中参数的值
                    switch (currentMode) {
                        case HSV_ADJUST_HUE:
                            // 处理显示值的调整，使用固定步长1
                            display_hue = (display_hue + 1) % 361; // 0-360度
                            // 转换回内部值并设置
                            h = (display_hue * 255) / 360;
                            kob_rgb_matrix_set_hsv(h, config->sat, config->val);
                            // 重新获取配置以确保一致性
                            config = kob_rgb_get_config();
                            break;
                        case HSV_ADJUST_SAT:
                            // 处理显示值的调整，使用固定步长1
                            display_sat = (display_sat + 1 > 100) ? 100 : (display_sat + 1); // 0-100%
                            // 转换回内部值并设置
                            s = (display_sat * 255) / 100;
                            kob_rgb_matrix_set_hsv(config->hue, s, config->val);
                            // 重新获取配置以确保一致性
                            config = kob_rgb_get_config();
                            break;
                        case HSV_ADJUST_VAL:
                            // 处理显示值的调整，使用固定步长1
                            display_val = (display_val + 1 > 100) ? 100 : (display_val + 1); // 0-100%
                            // 转换回内部值并设置
                            v = (display_val * 255) / 100;
                            kob_rgb_matrix_set_hsv(config->hue, config->sat, v);
                            // 重新获取配置以确保一致性
                            config = kob_rgb_get_config();
                            break;
                    }
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
            if (op != MENU_OP_ENTER && op != MENU_OP_BACK) {
                // 使用display_hue/sat/val变量而不是直接从config获取，确保显示值与用户调整的一致
                sprintf(hsv_str[0], "H: %d", display_hue);
                sprintf(hsv_str[1], "S: %d%%", display_sat);
                sprintf(hsv_str[2], "V: %d%%", display_val);
                
                OLED_Clear();
                // 根据OLED_6X8_HALF字体的建议垂直布局设置y坐标
                OLED_ShowString(12, 8, hsv_str[0], OLED_6X8_HALF);
                OLED_ShowString(12, 16, hsv_str[1], OLED_6X8_HALF);
                OLED_ShowString(12, 24, hsv_str[2], OLED_6X8_HALF);
                OLED_ShowString(0, 8 + currentMode * 8, " >", OLED_6X8_HALF); // 显示选中项标记
                OLED_Update();
            }
        }
    }
    
    MenuManager_DisplayMenu(get_menu_manager(), 0, 0, OLED_8X16_HALF);
}