/*
 * MIT许可证（MIT）
 *
 * 版权所有 (c) 2019 Ha Thach (tinyusb.org)
 *
 * 特此免费授予任何获得本软件及相关文档文件（以下简称"软件"）副本的人，不受限制地处理本软件，
 * 包括但不限于使用、复制、修改、合并、出版、发行、再许可和/或销售软件副本，
 * 以及允许向其提供软件的人做出上述行为，但须遵守以下条件：
 *
 * 上述版权声明和本许可声明应包含在软件的所有副本或主要部分中。
 *
 * 本软件按"原样"提供，不提供任何明示或暗示的担保，包括但不限于适销性、
 * 特定用途适用性和非侵权性的担保。在任何情况下，作者或版权持有人
 * 均不对任何索赔、损害或其他责任承担责任，无论是在合同诉讼、侵权行为或其他情况下，
 * 因软件或软件的使用或其他交易而产生的，均与本软件或使用或其他交易无关。
 *
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// USB端口号配置，默认为端口0
#define BOARD_TUD_RHPORT      0
// USB端口最大运行速度
#define BOARD_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED


//--------------------------------------------------------------------
// Common Configuration
//------------------------------------------------------------------
// MCU类型配置，用于选择 tinyusb 的 MCU 类型--
#define CFG_TUSB_MCU                OPT_MCU_ESP32S3
//用于定义 tinyusb 的操作系统，如果使用的是 FreeRTOS，需要启用该宏
#define CFG_TUSB_OS           OPT_OS_FREERTOS
// Espressif IDF要求包含路径中添加"freertos/"前缀
#define CFG_TUSB_OS_INC_PATH    freertos/
// 设为 1 启用 tinyusb device 功能
#define CFG_TUD_ENABLED       1
// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED
// 内存对齐声明 (4字节对齐)
#define CFG_TUSB_MEM_ALIGN        __attribute__ ((aligned(4)))
// 端点0的最大包大小
#define CFG_TUD_ENDPOINT0_SIZE    64


//------------- CLASS -------------//
#define CFG_TUD_HID               1
#define CFG_TUD_HID_EP_BUFSIZE    64

#define USB_VID               0x303A
// #define USB_PID               0   //在usb_driver.c里面会根据宏定义选择自动配置PID
#define USB_MANUFACTURER      "Espressif"
#define USB_PRODUCT           "HID Demo"

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
