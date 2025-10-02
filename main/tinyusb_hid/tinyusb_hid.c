/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_private/usb_phy.h"
#include "tinyusb_hid.h"
#include "usb_descriptors.h"
#include "device/usbd.h"
#include "keyboard_led/keyboard_led.h"

static const char *TAG = "tinyusb_hid.c";


static tinyusb_hid_t *s_tinyusb_hid = NULL;
extern bool s_remote_wakeup_enabled; // 跟踪远程唤醒功能是否被主机允许
bool s_remote_wakeup_enabled = false; // 全局变量定义

// 控制报告发送的标志
static bool s_report_enabled = true;


/**
 * @brief 初始化USB PHY
 * 
 * 配置USB物理层，设置为OTG设备模式
 */
static void usb_phy_init(void)
{
    usb_phy_handle_t phy_hdl;
    
    // 配置USB PHY为设备模式
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .target = USB_PHY_TARGET_INT
    };
    
    usb_new_phy(&phy_conf, &phy_hdl);
}

/**
 * @brief TinyUSB设备任务
 * 
 * 持续运行TinyUSB设备任务循环，处理USB事件
 */
static void tusb_device_task(void *arg)
{
    (void)arg;
    
    while (1) {
        tud_task();
    }
}

/**
 * @brief 上报键盘HID报告
 * 
 * 处理键盘按键报告，支持标准键盘报告和全键盘报告模式
 * 
 * @param report HID报告结构体
 */
void tinyusb_hid_keyboard_report(hid_report_t report)
{
    static bool use_full_key = false;
    
    // 检查是否需要远程唤醒
    if (tud_suspended()) {
        tud_remote_wakeup();
    } else {
        // 处理不同类型的报告
        switch (report.report_id) {
        case REPORT_ID_FULL_KEY_KEYBOARD:
            use_full_key = true;
            break;
        case REPORT_ID_KEYBOARD: {
            // 从全键盘模式切换回标准模式时，发送空的全键盘报告
            if (use_full_key) {
                hid_report_t _report = {0};
                _report.report_id = REPORT_ID_FULL_KEY_KEYBOARD;
                xQueueSend(s_tinyusb_hid->hid_queue, &_report, 0);
                use_full_key = false;
            }
            break;
        }
        default:
            break;
        }

        // 根据标志决定是否发送报告到队列
    if (s_report_enabled) {
        xQueueSend(s_tinyusb_hid->hid_queue, &report, 0);
    } else {
        ESP_LOGD(TAG, "HID report sending is disabled");
    }
    }
}

/**
 * @brief TinyUSB HID任务
 * 
 * 处理HID报告队列中的报告并发送到主机
 * 
 * @param arg 任务参数（未使用）
 */
static void tinyusb_hid_task(void *arg)
{
    (void) arg;
    hid_report_t report;
    
    while (1) {
        if (xQueueReceive(s_tinyusb_hid->hid_queue, &report, portMAX_DELAY)) {
            // 检查是否需要远程唤醒
            if (tud_suspended()) {
                tud_remote_wakeup();
                xQueueReset(s_tinyusb_hid->hid_queue);
            } else {
                // 根据报告ID处理不同类型的报告
                switch (report.report_id) {
                case REPORT_ID_KEYBOARD:
                    tud_hid_n_report(0, REPORT_ID_KEYBOARD, &report.keyboard_report, sizeof(report.keyboard_report));
                    break;
                case REPORT_ID_FULL_KEY_KEYBOARD:
                    tud_hid_n_report(0, REPORT_ID_FULL_KEY_KEYBOARD, &report.keyboard_full_key_report, sizeof(report.keyboard_full_key_report));
                    break;
                case REPORT_ID_CONSUMER:
                    tud_hid_n_report(0, REPORT_ID_CONSUMER, &report.consumer_report, sizeof(report.consumer_report));
                    break;
                default:
                    // 未知报告类型，跳过处理
                    continue;
                }
                
                // 等待报告发送完成
                if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100))) {
                    ESP_LOGW(TAG, "Report not sent");
                }
            }
        }
    }
}

/**
 * @brief 初始化TinyUSB HID设备
 * 
 * 此函数初始化USB PHY、TinyUSB设备、创建必要的队列和任务，
 * 并初始化Windows Lighting功能
 * 
 * @return
 *    - ESP_OK: 初始化成功
 *    - ESP_ERR_NO_MEM: 内存分配失败
 */
esp_err_t tinyusb_hid_init(void)
{
    // 检查是否已经初始化
    if (s_tinyusb_hid) {
        ESP_LOGW(TAG, "tinyusb_hid already initialized");
        return ESP_OK;
    }
    
    // 分配TinyUSB HID结构体内存
    esp_err_t ret = ESP_OK;
    s_tinyusb_hid = calloc(1, sizeof(tinyusb_hid_t));
    ESP_RETURN_ON_FALSE(s_tinyusb_hid, ESP_ERR_NO_MEM, TAG, "calloc failed");

    // 初始化USB PHY和TinyUSB设备
    usb_phy_init();
    tud_init(BOARD_TUD_RHPORT);

    // 创建HID报告队列
    s_tinyusb_hid->hid_queue = xQueueCreate(10, sizeof(hid_report_t));
    ESP_GOTO_ON_FALSE(s_tinyusb_hid->hid_queue, ESP_ERR_NO_MEM, fail, TAG, "xQueueCreate failed");
    
    // 初始化Windows Lighting功能
    windows_lighting_init();
    
    // 创建TinyUSB相关任务
    xTaskCreate(tusb_device_task, "TinyUSB", 4096, NULL, 5, NULL);
    xTaskCreate(tinyusb_hid_task, "tinyusb_hid_task", 4096, NULL, 5, &s_tinyusb_hid->task_handle);
    xTaskNotifyGive(s_tinyusb_hid->task_handle);
    
    return ret;
    
fail:
    free(s_tinyusb_hid);
    s_tinyusb_hid = NULL;
    return ret;
}

/************************************************** TinyUSB callbacks ***********************************************/
// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t itf, uint8_t const *report, uint16_t len)
{
    (void) itf;
    (void) len;

    xTaskNotifyGive(s_tinyusb_hid->task_handle);
}

/************************************************** Windows Lighting **********************************************/
// Windows Lighting 灯的位置和颜色数据
static int32_t lamp_positions[WS2812B_NUM][3]; // 灯的位置坐标(仅内部使用)
uint8_t lamp_colors[WS2812B_NUM][4];           // RGBA颜色数据(全局变量，供keyboard_led.c访问)
bool autonomous_mode = false;                  // 自主模式标志(全局变量，供keyboard_led.c访问)

// 声明从keyboard_led.c获取的互斥锁，用于保护共享资源访问
extern SemaphoreHandle_t g_windows_lighting_mutex;

/**
 * @brief 初始化灯的位置信息
 * 
 * 设置键盘上每个LED的坐标位置，用于Windows Lighting功能
 */

static void init_lamp_positions(void) {
    // 设置默认的灯位置信息，这些坐标与实际键盘布局相匹配
    const int32_t default_lamp_positions[WS2812B_NUM][3] = {
        {10000, 10000, 0},  // LED 0
        {20000, 10000, 0},  // LED 1
        {30000, 10000, 0},  // LED 2
        {40000, 10000, 0},  // LED 3
        {50000, 10000, 0},  // LED 4
        {60000, 10000, 0},  // LED 5
        {70000, 10000, 0},  // LED 6
        {80000, 10000, 0},  // LED 7
        {15000, 20000, 0},  // LED 8
        {25000, 20000, 0},  // LED 9
        {35000, 20000, 0},  // LED 10
        {45000, 20000, 0},  // LED 11
        {55000, 20000, 0},  // LED 12
        {65000, 20000, 0},  // LED 13
        {20000, 30000, 0},  // LED 14
        {30000, 30000, 0},  // LED 15
        {40000, 30000, 0},  // LED 16
    };
    
    // 复制默认位置信息
    memcpy(lamp_positions, default_lamp_positions, sizeof(lamp_positions));
}

/**
 * @brief 初始化Windows Lighting相关功能
 * 
 * 在USB设备初始化时调用，为键盘提供Windows Lighting支持
 */
void windows_lighting_init(void) {
    init_lamp_positions();
}

/**
 * @brief 处理HID获取报告请求
 * 
 * 应用程序必须填充缓冲区报告内容并返回其长度
 * 返回零将导致堆栈STALL请求
 * 
 * @param itf 接口编号
 * @param report_id 报告ID
 * @param report_type 报告类型
 * @param buffer 用于填充报告内容的缓冲区
 * @param reqlen 请求的长度
 * @return 填充的报告长度，返回0将导致请求被STALL
 */
/**
 * @brief 控制HID报告发送
 * 
 * @param enable true表示启用报告发送，false表示禁用报告发送
 */
void tinyusb_hid_enable_report(bool enable)
{
    s_report_enabled = enable;
    ESP_LOGI(TAG, "HID report sending %s", enable ? "enabled" : "disabled");
    
    // 如果禁用报告发送，清空队列中的所有报告
    if (!enable && s_tinyusb_hid && s_tinyusb_hid->hid_queue) {
        xQueueReset(s_tinyusb_hid->hid_queue);
    }
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void) itf;
    (void) report_type;
    (void) reqlen;

    switch (report_id) {
    case REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES: {
        // 填充灯阵列属性报告
        uint16_t *report = (uint16_t *)buffer;
        report[0] = WS2812B_NUM;  // 灯的数量
        
        // 填充其他属性
        int32_t *report32 = (int32_t *)(buffer + 2);
        report32[0] = 80000;  // 边界框宽度
        report32[1] = 30000;  // 边界框高度
        report32[2] = 0;      // 边界框深度
        report32[3] = LAMP_ARRAY_KIND_KEYBOARD;  // 灯阵列类型
        report32[4] = 10000;  // 最小更新间隔
        
        return 22;  // 报告长度
    }
    
    case REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_RESPONSE: {
        // 获取灯ID
        uint16_t lamp_id = ((uint16_t *)buffer)[0];
        if (lamp_id >= WS2812B_NUM) {
            return 0;  // 无效的灯ID
        }
        
        // 填充灯属性响应报告
        ((uint16_t *)buffer)[0] = lamp_id;  // 灯ID
        
        // 填充位置信息
        int32_t *report32 = (int32_t *)(buffer + 2);
        report32[0] = lamp_positions[lamp_id][0];  // X坐标
        report32[1] = lamp_positions[lamp_id][1];  // Y坐标
        report32[2] = lamp_positions[lamp_id][2];  // Z坐标
        report32[3] = 1000;  // 更新延迟
        report32[4] = 0x00000001;  // 灯的用途（键盘按键）
        
        // 填充颜色通道信息
        uint8_t *report8 = buffer + 22;
        report8[0] = 255;  // 红色级别数
        report8[1] = 255;  // 绿色级别数
        report8[2] = 255;  // 蓝色级别数
        report8[3] = 255;  // 亮度级别数
        report8[4] = 1;    // 是否可编程
        report8[5] = 0;    // 输入绑定
        
        return 28;  // 报告长度
    }
    
    default:
        // 不支持的报告ID
        return 0;
    }
}

/**
 * @brief 处理HID设置报告请求
 * 
 * 主要处理Windows Lighting相关的灯效设置请求
 */
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void) itf;
    (void) report_type;
    (void) bufsize;

    switch (report_id) {
    case REPORT_ID_LIGHTING_LAMP_MULTI_UPDATE: {
        // 多灯更新报告
        uint8_t lamp_count = buffer[0];
        (void)buffer[1]; // 未使用的update_flags
        
        if (lamp_count > 8) lamp_count = 8;  // 最多8个灯
        
        // 获取互斥锁，保护共享资源访问
        if (g_windows_lighting_mutex && xSemaphoreTake(g_windows_lighting_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (uint8_t i = 0; i < lamp_count; i++) {
                uint16_t lamp_id = ((uint16_t *)&buffer[2])[i];
                if (lamp_id >= WS2812B_NUM) continue;
                
                // 获取灯的颜色信息
                uint8_t red = buffer[18 + i * 4];
                uint8_t green = buffer[19 + i * 4];
                uint8_t blue = buffer[20 + i * 4];
                uint8_t intensity = buffer[21 + i * 4];
                
                // 更新灯的颜色
                lamp_colors[lamp_id][0] = red;
                lamp_colors[lamp_id][1] = green;
                lamp_colors[lamp_id][2] = blue;
                lamp_colors[lamp_id][3] = intensity;
            }
            
            // 释放互斥锁
            xSemaphoreGive(g_windows_lighting_mutex);
            
            // 如果不是自主模式，记录更新信息
            if (!autonomous_mode) {
                ESP_LOGD(TAG, "Updated %d lamps in Windows Lighting mode", lamp_count);
            }
        } else {
            ESP_LOGW(TAG, "Failed to acquire mutex for Windows Lighting multi update");
        }
        break;
    }
    
    case REPORT_ID_LIGHTING_LAMP_RANGE_UPDATE: {
        // 灯范围更新报告
        (void)buffer[0]; // 未使用的update_flags
        uint16_t start_lamp_id = ((uint16_t *)&buffer[1])[0];
        uint16_t end_lamp_id = ((uint16_t *)&buffer[3])[0];
        
        // 获取颜色信息
        uint8_t red = buffer[5];
        uint8_t green = buffer[6];
        uint8_t blue = buffer[7];
        uint8_t intensity = buffer[8];
        
        // 确保ID在有效范围内
        if (start_lamp_id >= WS2812B_NUM) start_lamp_id = WS2812B_NUM - 1;
        if (end_lamp_id >= WS2812B_NUM) end_lamp_id = WS2812B_NUM - 1;
        
        // 获取互斥锁，保护共享资源访问
        if (g_windows_lighting_mutex && xSemaphoreTake(g_windows_lighting_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // 更新范围内的所有灯
            for (uint16_t i = start_lamp_id; i <= end_lamp_id; i++) {
                lamp_colors[i][0] = red;
                lamp_colors[i][1] = green;
                lamp_colors[i][2] = blue;
                lamp_colors[i][3] = intensity;
            }
            
            // 释放互斥锁
            xSemaphoreGive(g_windows_lighting_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire mutex for Windows Lighting range update");
        }
        break;
    }
    
    case REPORT_ID_LIGHTING_LAMP_ARRAY_CONTROL: {
        // 灯阵列控制报告
        // autonomous_mode 可以在这里直接修改，因为它是布尔值，原子操作
        autonomous_mode = (buffer[0] != 0);
        ESP_LOGD(TAG, "Windows Lighting autonomous mode %s", autonomous_mode ? "enabled" : "disabled");
        break;
    }
    
    default:
        // 其他报告类型不处理
        break;
    }
}

/**
 * @brief 设备挂载回调函数
 */
void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "USB Mount");
}

/**
 * @brief 设备卸载回调函数
 */
void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "USB Un-Mount");
}

// 全局变量，用于保存USB挂起前的WS2812状态
static bool s_saved_ws2812_state = false;

/**
 * @brief USB总线挂起回调函数
 * 
 * @param remote_wakeup_en 是否允许执行远程唤醒
 * 
 * 当USB总线挂起时，设备必须在7ms内将平均电流降低到2.5mA以下
 */
void tud_suspend_cb(bool remote_wakeup_en)
{
    s_remote_wakeup_enabled = remote_wakeup_en;
    ESP_LOGI(TAG, "USB Suspended - Remote wakeup allowed: %s", remote_wakeup_en ? "YES" : "NO");
    
    // 保存当前WS2812状态并关闭灯光效果以节省电量
    s_saved_ws2812_state = kob_ws2812_is_enable();
    kob_ws2812_enable(false);
}

/**
 * @brief USB总线恢复回调函数
 */
void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB Resume");
    
    // 主机苏醒时恢复之前保存的WS2812状态
    kob_ws2812_enable(s_saved_ws2812_state);
}
