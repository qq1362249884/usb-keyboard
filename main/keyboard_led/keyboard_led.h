#ifndef _KEYBOARD_LED_
#define _KEYBOARD_LED_

//头文件位置
#include "string.h"
#include "stdio.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"


#include "led_strip.h"
#include "rgb_matrix_drivers.h"
#include "rgb_matrix.h"

//宏定义位置
#define WS2812B_POWER_PIN   14
#define WS2812B_DATA_PIN    17
#define WS2812B_NUM         17


//函数定义位置
esp_err_t kob_ws2812b_init(led_strip_handle_t *led_strip);
esp_err_t kob_ws2812_enable(bool enable);
esp_err_t kob_ws2812_clear(void);
bool kob_ws2812_is_enable(void);
esp_err_t kob_rgb_matrix_init(void);
void appLedStart(void);

#endif