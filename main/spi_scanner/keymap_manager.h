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
#include "spi_config.h"
#include "spi_scanner.h"
#include "keymap_nvs_manager.h"

// 运行时按键映射（可通过NVS修改）
extern uint16_t keymaps[2][NUM_KEYS];

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

#ifdef __cplusplus
}
#endif

#endif // _KEYMAP_MANAGER_H_