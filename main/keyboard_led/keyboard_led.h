#ifndef _KEYBOARD_LED_
#define _KEYBOARD_LED_

//头文件位置
#include "string.h"
#include "stdio.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_strip.h"
#include "rgb_matrix_drivers.h"
#include "rgb_matrix.h"
#include "nvs_manager/nvs_manager.h"

//宏定义位置
#define WS2812B_POWER_PIN   0
#define WS2812B_DATA_PIN    17
#define WS2812B_NUM         17

// 使用软件控制灯珠亮度为0代替关闭电源
#define KOB_WS2812_USE_SOFTWARE_POWER_OFF 1

// 默认灯效配置
#define DEFAULT_RGB_MODE     1
#define DEFAULT_RGB_HUE      0
#define DEFAULT_RGB_SAT      255
#define DEFAULT_RGB_VAL      128
#define DEFAULT_RGB_SPEED    100

// 添加Windows Lighting模式
#define RGB_MODE_WINDOWS_LIGHTING  RGB_MATRIX_EFFECT_MAX  

// LED灯效配置结构体
typedef struct {
    uint16_t mode;       // 灯效模式
    uint8_t hue;         // 色调
    uint8_t sat;         // 饱和度
    uint8_t val;         // 亮度
    uint8_t speed;       // 速度
    bool enabled;        // 是否启用
} led_effect_config_t;

//函数定义位置
esp_err_t kob_ws2812b_init(led_strip_handle_t *led_strip);
esp_err_t kob_ws2812_enable(bool enable);
esp_err_t kob_ws2812_clear(void);
bool kob_ws2812_is_enable(void);
esp_err_t kob_rgb_matrix_init(void);
void led_task(void);

// 新增的灯效控制函数
esp_err_t kob_rgb_matrix_set_mode(uint16_t mode);
esp_err_t kob_rgb_matrix_set_hsv(uint8_t hue, uint8_t sat, uint8_t val);
esp_err_t kob_rgb_matrix_set_speed(uint8_t speed);
esp_err_t kob_rgb_matrix_next_mode(void);
esp_err_t kob_rgb_matrix_prev_mode(void);

// 配置管理函数
esp_err_t kob_rgb_save_config(void);
esp_err_t kob_rgb_load_config(void);
led_effect_config_t* kob_rgb_get_config(void);

// OLED菜单动作函数
esp_err_t kob_rgb_matrix_increase_hue(void);
esp_err_t kob_rgb_matrix_decrease_hue(void);
esp_err_t kob_rgb_matrix_increase_sat(void);
esp_err_t kob_rgb_matrix_decrease_sat(void);
esp_err_t kob_rgb_matrix_increase_val(void);
esp_err_t kob_rgb_matrix_decrease_val(void);
esp_err_t kob_rgb_matrix_increase_speed(void);
esp_err_t kob_rgb_matrix_decrease_speed(void);

// 键盘响应处理函数
void kob_rgb_process_key_event(uint8_t row, uint8_t col, bool pressed);

#endif