#ifndef _SPI_SCANNER_
#define _SPI_SCANNER_

//头文件位置
#include "string.h"
#include "stdio.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h" // for esp_rom_delay_us
#include "esp_timer.h" // for esp_timer_get_time
#include "unistd.h"

#include "keycodes.h"
#include "usb_descriptors.h"
#include "tinyusb_hid.h"

//宏定义位置
#define SPI_HOST    SPI2_HOST   // 使用 SPI2 主机
#define PIN_NUM_QH   10         //74hc165 QH引脚
#define PIN_NUM_SCLK 11         //74hc165 CLK引脚
#define PIN_NUM_PL   12         //74hc165 PL引脚            
#define NUM_KEYS     17         //按键数量
#define NUM_BYTES    3          //165数量

//结构体定义
typedef struct
{
    uint16_t key_pressed_num;
    uint16_t key_release_num;
    uint16_t key_pressed_data[];
} keymap_t;


//函数定义位置
void spi_hid_init(void);
extern void test_spi_task(void *pvParameter);


#endif