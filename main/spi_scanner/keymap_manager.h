#ifndef _KEYMAP_MANAGER_H_
#define _KEYMAP_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "spi_keyboard_config.h"
#include "spi_scanner.h"
#include "nvs_manager/unified_nvs_manager.h"

// 组合键标志位定义（真正完美方案：使用用户自定义范围0x7000作为组合键标志，完全避开所有标准键码和修饰键范围）
#define KEY_COMBO_FLAG 0x7000    // 组合键标志位（使用用户自定义范围0x7000，完全避开标准键码和修饰键范围）
#define KEY_MODIFIER_MASK 0x0F00  // 修饰键掩码（覆盖0x0100-0x0F00范围，避开组合键标志位）
#define KEY_BASE_MASK 0x00FF     // 基础键码掩码（低8位，标准基础键码范围）

// 修饰键定义（简化方案：只保留左修饰键，避免范围冲突）
#define MOD_LCTRL  0x0100
#define MOD_LSHIFT 0x0200
#define MOD_LALT   0x0400
#define MOD_LGUI   0x0800

// 层数定义
#define TOTAL_LAYERS 7            // 总层数：层0（默认）+ 层1-6（自定义）
#define DEFAULT_LAYER 0           // 默认层（不可修改）
#define FIRST_CUSTOM_LAYER 1      // 第一个自定义层
#define LAST_CUSTOM_LAYER 6       // 最后一个自定义层

// 运行时按键映射（可通过NVS修改）
extern uint16_t keymaps[TOTAL_LAYERS][NUM_KEYS];

/**
 * @brief 检查是否为组合键
 * @param keycode 键码
 * @return true 是组合键，false 不是组合键
 */
bool is_combo_key(uint16_t keycode);

/**
 * @brief 获取组合键的基础键码
 * @param keycode 组合键码
 * @return 基础键码
 */
uint16_t get_base_key(uint16_t keycode);

/**
 * @brief 获取组合键的修饰键掩码
 * @param keycode 组合键码
 * @return 修饰键掩码
 */
uint16_t get_modifier_mask(uint16_t keycode);

/**
 * @brief 创建组合键
 * @param base_key 基础键码
 * @param modifier_mask 修饰键掩码
 * @return 组合键码
 */
uint16_t create_combo_key(uint16_t base_key, uint16_t modifier_mask);

/**
 * @brief 初始化NVS并加载按键映射
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t nvs_keymap_init(void);

/**
 * @brief 保存按键映射到NVS
 * @param layer 层索引
 * @param keymap 按键映射数组
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t save_keymap_to_nvs(uint8_t layer, const uint16_t *keymap);

/**
 * @brief 从NVS加载按键映射
 * @param layer 层索引
 * @param keymap 用于存储按键映射的数组
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t load_keymap_from_nvs(uint8_t layer, uint16_t *keymap);

/**
 * @brief 将按键映射重置为默认值
 * @param layer 层索引
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t reset_keymap_to_default(uint8_t layer);

/**
 * @brief 保存单个按键到NVS
 * @param layer 层索引
 * @param key_index 按键索引
 * @param key_code 键码
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t save_single_key_to_nvs(uint8_t layer, uint8_t key_index, uint16_t key_code);

/**
 * @brief 测试按键映射配置功能
 * 这个函数演示如何修改、保存和加载按键映射
 */
void test_keymap_config(void);

/**
 * @brief 清理NVS管理器资源
 * 这个函数应该在程序退出时调用，释放NVS管理器资源
 */
void nvs_keymap_cleanup(void);

/**
 * @brief 按键映射测试任务
 * 这个任务演示如何使用NVS功能来修改、保存和加载按键映射
 */
void keymap_test_task(void *pvParameter);

/**
 * @brief 启动按键映射测试
 */
void start_keymap_test(void);

/**
 * @brief 设置NVS管理器实例
 * 
 * 用于外部模块设置NVS管理器实例，避免多个实例冲突
 * 
 * @param manager NVS管理器实例
 */
void set_nvs_manager(unified_nvs_manager_t* manager);

#ifdef __cplusplus
}
#endif

#endif // _KEYMAP_MANAGER_H_