#ifndef _JOYSTICK_H_
#define _JOYSTICK_H_

#include "string.h"
#include "stdio.h"
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"


#define JOYSTICK_SW_PIN 3

// 定义方向和旋转的枚举类型
// 采用欧几里得距离动态判断方向，提高准确性和灵活性
typedef enum {
    JOYSTICK_CENTER = 0,
    JOYSTICK_UP,
    JOYSTICK_DOWN,
    JOYSTICK_LEFT,
    JOYSTICK_RIGHT
} joystick_direction_t;

// 定义按键按下的类型
typedef enum {
    BUTTON_NONE = 0,
    BUTTON_SHORT_PRESS,
    BUTTON_DOUBLE_PRESS,
    BUTTON_LONG_PRESS
} button_press_type_t;

// 定义摇杆状态结构体，包含方向和按键状态
typedef struct {
    joystick_direction_t direction;  // 摇杆方向
    button_press_type_t press_type;  // 按键按下的类型
} joystick_state_t;

// 函数声明
joystick_state_t get_joystick_direction();
void sw_gpio_init(void);
button_press_type_t detect_button_press();


#endif