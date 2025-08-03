/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */


 /*
    标准的描述符有5种，USB为这些描述符定义了编号：
    1——设备描述符
    2——配置描述符
    3——字符串描述符
    4——接口描述符
    5——端点描述符
    0x21——HID描述符
    0x22——报表描述符/报告描述符
 */


#include "tusb.h"
#include "usb_descriptors.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 * 
 * Auto ProductID layout's Bitmap:
 *   [MSB]  VIDEO | AUDIO | MIDI | HID | MSC | CDC          [LSB]
 * 自动根据tusb_config.h中的宏定义配置pid
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#ifndef USB_PID
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
    _PID_MAP(MIDI, 3) | _PID_MAP(AUDIO, 4) | _PID_MAP(VIDEO, 5) | _PID_MAP(VENDOR, 6) )
#endif

//--------------------------------------------------------------------+
//  设备描述符
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),//设备描述符的字节数大小
    .bDescriptorType    = TUSB_DESC_DEVICE,          //描述符类型编号
    .bcdUSB             = 0x0200,                    //USB版本号

    // Use Interface Association Descriptor (IAD) for Video
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,          //USB分配的设备类代码，0x01~0xfe为标准设备类，0xff为厂商自定义类型，0x00不是在设备描述符中定义的，如HID 
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,     //USB分配的设备子类代码
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,        //USB分配的设备协议代码

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,   //端点0最大包长度

    .idVendor           = USB_VID,                  //厂商ID
    .idProduct          = USB_PID,                  //产品ID
    .bcdDevice          = 0x0100,                   //设备版本号

    .iManufacturer      = 0x01,                     //厂商字符串索引    
    .iProduct           = 0x02,                     //产品字符串索引
    .iSerialNumber      = 0x03,                     //序列号字符串索引

    .bNumConfigurations = 0x01                       //配置数量
};

//hid描述符/报告描述符
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_FULL_KEY_KEYBOARD(HID_REPORT_ID(REPORT_ID_FULL_KEY_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_ID_CONSUMER)),
    TUD_HID_REPORT_DESC_LIGHTING(REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES)
};

//hid描述符长度
const uint16_t desc_hid_report_len = sizeof(desc_hid_report);

//获取HID报告描述符
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void) instance;
    return desc_hid_report;
}

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
//获取设备描述符
uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// 配置描述符
//--------------------------------------------------------------------+
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN * CFG_TUD_HID)

uint8_t const desc_fs_configuration[] = {
    //配置描述符 配置编号、接口数量、字符串索引、总长度、属性、功率（毫安）
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    //配置描述符 接口编号、字符串索引、协议、报告描述符长度、端点输入地址、大小和轮询间隔。
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 4, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), (0x80 | EPNUM_HID_DATA), CFG_TUD_HID_EP_BUFSIZE, 1)
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
//获取配置描述符
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index; // for multiple configurations

    return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const *string_desc_arr [] = {
    (const char[]) { 0x09, 0x04 }, // 0: 语言列表 英语(0x0409)
    USB_MANUFACTURER,              // 1: 厂商名称
    USB_PRODUCT,                   // 2: 产品名称
    "123456",                      // 3: 序列号
    "HID",                         // 4: 接口名称
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid;

    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
            return NULL;
        }

        const char *str = string_desc_arr[index];

        // Cap at max char
        chr_count = (uint8_t) strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

    return _desc_str;
}
