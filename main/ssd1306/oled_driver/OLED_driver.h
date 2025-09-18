#ifndef __OLED_DRIVER_H
#define __OLED_DRIVER_H

#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdarg.h>
#include "stdio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"


// 芯片型号选择
#define SSD1306
// #define SH1106

//SSD1306引脚定义
#define OLED_SCL_Pin 14
#define OLED_SDA_Pin 13
#define OLED_ADDRESS 0x3C



#define OLED_CMD 0  // 写命令
#define OLED_DATA 1 // 写数据

//	oled初始化函数
void OLED_Init(void);

//	oled全屏刷新函数
void OLED_Update(void);
//	oled局部刷新函数
void OLED_UpdateArea(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height);
// 设置颜色模式
void OLED_SetColorMode(bool colormode);
// OLED 屏幕亮度函数
void OLED_Brightness(int16_t Brightness);

#endif
