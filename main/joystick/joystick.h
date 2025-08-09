#ifndef _JOYSTICK_H_
#define _JOYSTICK_H_

#include "string.h"
#include "stdio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define joystick_x_pin 14
#define joystick_y_pin 15
#define joystick_sw_pin 16

void joystick_task();

#endif