#ifndef _SPI_SCANNER_
#define _SPI_SCANNER_

//头文件位置
#include "string.h"
#include "stdio.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h" // for esp_rom_delay_us


#include "spi_config.h"
#include "keycodes.h"

#include "usb_descriptors.h"
#include "tinyusb_hid.h"
#include "keymap_manager.h"


//结构体定义
typedef struct
{
    uint16_t key_pressed_num;
    uint16_t key_release_num;
    uint16_t key_pressed_data[];
} keymap_t;


//函数定义位置
void spi_scanner_keyboard_task(void);




#endif