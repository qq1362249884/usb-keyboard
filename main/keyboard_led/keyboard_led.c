#include "keyboard_led.h"
#include "rgb_matrix_nvs.h"
#include "nvs_manager/unified_nvs_manager.h"
#include <inttypes.h>

//全局变量定义
static const char *TAG = "app_led";
static led_strip_handle_t s_led_strip = NULL;
static bool s_led_enable = false;

// LED配置结构体
led_config_t g_led_config = 
{
    {
        // Key Matrix to LED Index
        // 根据实际硬件连接顺序调整索引
        {0, 1, 2, 3},
        {4, 5, 6, 7},
        {8, 9, 10,NO_LED},
        {11, 12, 13, NO_LED},
        {14,NO_LED, 15,16}
    },
    {
        // LED Index to Physical Position
        {0, 0}, {9, 0}, {27, 0}, {45, 0},        // 第一行
        {0, 9}, {9, 9}, {27, 9}, {45, 13},        // 第二行
        {0, 27}, {9, 27}, {27, 27},              // 第三行
        {0, 45}, {9, 45}, {27, 45},              // 第四行
        {13, 63}, {27, 63}, {45, 54}               // 第五行
    },
    {
        // LED Index to Flag
        // 根据需要设置标志位
        4, 4, 4, 4,
        4, 4, 4, 4,
        4, 4, 4,
        4, 4, 4, 
        4, 4, 4
    }
};

// 全局灯效配置结构体
static led_effect_config_t g_led_effect_config = {
    .mode = DEFAULT_RGB_MODE,
    .hue = DEFAULT_RGB_HUE,
    .sat = DEFAULT_RGB_SAT,
    .val = DEFAULT_RGB_VAL,
    .speed = DEFAULT_RGB_SPEED,
    .enabled = false
};

// 统一NVS管理器实例
static unified_nvs_manager_t* g_unified_nvs_manager = NULL;

// 获取配置结构体指针
led_effect_config_t* kob_rgb_get_config(void)
{
    return &g_led_effect_config;
}

// 设置统一NVS管理器实例
void kob_rgb_set_nvs_manager(unified_nvs_manager_t* manager)
{
    g_unified_nvs_manager = manager;
}

// 保存配置到NVS
esp_err_t kob_rgb_save_config(void)
{
    ESP_LOGI(TAG, "Saving RGB matrix configuration to NVS");
    
    esp_err_t ret = ESP_OK;
    
    if (!g_unified_nvs_manager) {
        ESP_LOGE(TAG, "Unified NVS manager not initialized");
        return ESP_FAIL;
    }
    
    // 保存灯效模式 - 使用正确的数据类型
    ret = UNIFIED_NVS_SAVE_U16(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_mode", g_led_effect_config.mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save RGB matrix mode: %s", esp_err_to_name(ret));
    }
    
    // 保存HSV值 - 使用正确的数据类型
    ret |= UNIFIED_NVS_SAVE_U8(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_hue", g_led_effect_config.hue);
    ret |= UNIFIED_NVS_SAVE_U8(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_sat", g_led_effect_config.sat);
    ret |= UNIFIED_NVS_SAVE_U8(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_val", g_led_effect_config.val);
    
    // 保存速度 - 使用正确的数据类型
    ret |= UNIFIED_NVS_SAVE_U8(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_speed", g_led_effect_config.speed);
    
    // 保存启用状态
    ret |= UNIFIED_NVS_SAVE_BOOL(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_enabled", g_led_effect_config.enabled);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "RGB matrix configuration saved successfully");
    } else {
        ESP_LOGE(TAG, "Failed to save some RGB matrix configuration parameters");
    }
    
    return ret;
}

// 从NVS加载配置
esp_err_t kob_rgb_load_config(void)
{
    ESP_LOGI(TAG, "Loading RGB matrix configuration from NVS");
    
    esp_err_t ret = ESP_OK;
    uint16_t mode = 0;
    uint8_t hue = 0, sat = 0, val = 0, speed = 0;
    bool enabled = false;
    
    if (!g_unified_nvs_manager) {
        ESP_LOGE(TAG, "Unified NVS manager not initialized");
        return ESP_FAIL;
    }
    
    // 加载灯效模式 - 使用正确的数据类型
    if (UNIFIED_NVS_LOAD_U16(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_mode", &mode) == ESP_OK) {
        g_led_effect_config.mode = mode;
    } else {
        ESP_LOGW(TAG, "RGB matrix mode not found in NVS, using default: %d", DEFAULT_RGB_MODE);
        g_led_effect_config.mode = DEFAULT_RGB_MODE;
        ret = ESP_ERR_NOT_FOUND;
    }
    
    // 加载HSV值 - 使用正确的数据类型
    if (UNIFIED_NVS_LOAD_U8(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_hue", &hue) == ESP_OK) {
        g_led_effect_config.hue = hue;
    } else {
        g_led_effect_config.hue = DEFAULT_RGB_HUE;
    }
    
    if (UNIFIED_NVS_LOAD_U8(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_sat", &sat) == ESP_OK) {
        g_led_effect_config.sat = sat;
    } else {
        g_led_effect_config.sat = DEFAULT_RGB_SAT;
    }
    
    if (UNIFIED_NVS_LOAD_U8(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_val", &val) == ESP_OK) {
        g_led_effect_config.val = val;
    } else {
        g_led_effect_config.val = DEFAULT_RGB_VAL;
    }
    
    // 加载速度 - 使用正确的数据类型
    if (UNIFIED_NVS_LOAD_U8(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_speed", &speed) == ESP_OK) {
        g_led_effect_config.speed = speed;
    } else {
        g_led_effect_config.speed = DEFAULT_RGB_SPEED;
    }
    
    // 加载启用状态
    if (UNIFIED_NVS_LOAD_BOOL(g_unified_nvs_manager, NVS_NAMESPACE_SYSTEM, "rgb_enabled", &enabled) == ESP_OK) {
        g_led_effect_config.enabled = enabled;
    } else {
        g_led_effect_config.enabled = false;
    }
    
    return ret;
}

// Windows Lighting相关定义
// 使用WS2812B_NUM常量代替定义MAX_LAMPS，避免宏定义冲突

// 声明外部变量，用于访问Windows Lighting的颜色数据
extern uint8_t lamp_colors[WS2812B_NUM][4];
extern bool autonomous_mode;

// 用于保护共享资源的互斥锁，声明为全局变量以便tinyusb_hid.c访问
SemaphoreHandle_t g_windows_lighting_mutex = NULL;

// Windows Lighting模式的LED更新函数
static void windows_lighting_update(void)
{
    if (!kob_ws2812_is_enable()) {
        return;
    }
    
    // 检查是否处于Windows Lighting模式且不在自主模式
    if (g_led_effect_config.mode == RGB_MODE_WINDOWS_LIGHTING && !autonomous_mode) {
        // 检查s_led_strip是否有效，避免空指针访问
        if (!s_led_strip) {
            ESP_LOGE(TAG, "s_led_strip is NULL in windows_lighting_update");
            return;
        }
        
        // 创建一个本地缓冲区，用于临时存储颜色数据，减少对共享资源的锁定时间
        // 使用__attribute__((aligned(4)))确保4字节对齐，避免SPI传输时的内存问题
        uint8_t local_colors[WS2812B_NUM][4] __attribute__((aligned(4)));
        
        // 获取互斥锁，保护共享资源访问
        if (g_windows_lighting_mutex && xSemaphoreTake(g_windows_lighting_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // 复制颜色数据到本地缓冲区
            memcpy(local_colors, lamp_colors, sizeof(lamp_colors));
            // 释放互斥锁
            xSemaphoreGive(g_windows_lighting_mutex);
        } else {
            // 如果无法获取锁，跳过本轮更新
            ESP_LOGW(TAG, "Failed to acquire mutex for Windows Lighting update");
            return;
        }
        
        // 使用本地缓冲区的数据更新LED，避免在操作LED时持有锁
        for (uint8_t i = 0; i < WS2812B_NUM; i++) {
            // 应用Windows Lighting的颜色设置
            uint8_t red = local_colors[i][0];
            uint8_t green = local_colors[i][1];
            uint8_t blue = local_colors[i][2];
            uint8_t intensity = local_colors[i][3];
            
            // 考虑亮度设置
            red = (uint8_t)((red * intensity) / 255);
            green = (uint8_t)((green * intensity) / 255);
            blue = (uint8_t)((blue * intensity) / 255);
            
            // 设置LED颜色
            led_strip_set_pixel(s_led_strip, i, red, green, blue);
        }
        
        // 添加增强的错误处理和重试机制，防止SPI传输失败导致崩溃
        esp_err_t err;
        int retry_count = 0;
        const int max_retries = 2;
        
        do {
            err = led_strip_refresh(s_led_strip);
            if (err == ESP_OK) {
                break;  // 传输成功，退出循环
            }
            
            ESP_LOGE(TAG, "Failed to refresh LED strip (attempt %d/%d): %s", 
                     retry_count + 1, max_retries, esp_err_to_name(err));
            
            // 短暂延时后重试
            if (retry_count < max_retries - 1) {
                vTaskDelay(1 / portTICK_PERIOD_MS);
            }
        } while (++retry_count < max_retries);
        
        // 如果多次尝试后仍然失败，考虑重新初始化LED strip
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Multiple failed attempts to refresh LED strip, reinitializing...");
            // 尝试重新初始化，但不阻塞主循环
            led_strip_handle_t temp_strip;
            if (kob_ws2812b_init(&temp_strip) == ESP_OK) {
                ESP_LOGI(TAG, "Successfully reinitialized LED strip");
            }
        }
    }
}

// 外部声明Windows Lighting初始化函数
extern void windows_lighting_init(void);

// 初始化Windows Lighting功能，包括创建互斥锁
void kob_windows_lighting_init(void)
{
    // 创建互斥锁，用于保护共享资源访问
    if (!g_windows_lighting_mutex) {
        g_windows_lighting_mutex = xSemaphoreCreateMutex();
        if (!g_windows_lighting_mutex) {
            ESP_LOGE(TAG, "Failed to create Windows Lighting mutex");
        }
    }
}

// 设置灯效模式
esp_err_t kob_rgb_matrix_set_mode(uint16_t mode)
{
    // 检查是否为Windows Lighting模式
    if (mode == RGB_MODE_WINDOWS_LIGHTING) {
        g_led_effect_config.mode = mode;
        ESP_LOGI(TAG, "RGB matrix mode set to Windows Lighting");
        
        // 初始化我们自己的Windows Lighting功能，包括互斥锁
        kob_windows_lighting_init();
        
        // 清除当前LED显示，为Windows控制做准备
        if (kob_ws2812_is_enable()) {
            // 添加错误处理
            esp_err_t err = kob_ws2812_clear();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to clear WS2812: %s", esp_err_to_name(err));
            }
        }
    } else {
        // 对于其他模式，进行范围检查
        uint16_t max_mode = RGB_MATRIX_EFFECT_MAX - 1; // 假设MAX是包含上限的，所以减1
        if (mode < 1 || mode > max_mode) {
            ESP_LOGW(TAG, "Invalid mode index %d, using default: %d", mode, DEFAULT_RGB_MODE);
            mode = DEFAULT_RGB_MODE;
        }
        
        g_led_effect_config.mode = mode;
        
        // 设置RGB矩阵模式
        rgb_matrix_mode(mode);
    }
    
    // 自动保存配置
    kob_rgb_save_config();
    
    ESP_LOGI(TAG, "RGB matrix mode set to: %d", g_led_effect_config.mode);
    return ESP_OK;
}

// 设置HSV值
esp_err_t kob_rgb_matrix_set_hsv(uint8_t hue, uint8_t sat, uint8_t val)
{
    g_led_effect_config.hue = hue;
    g_led_effect_config.sat = sat;
    g_led_effect_config.val = val;
    
    rgb_matrix_sethsv(hue, sat, val);
    
    // 自动保存配置
    kob_rgb_save_config();
    
    ESP_LOGI(TAG, "RGB matrix HSV set to: H=%d, S=%d, V=%d", hue, sat, val);
    return ESP_OK;
}

// 设置速度
esp_err_t kob_rgb_matrix_set_speed(uint8_t speed)
{
    g_led_effect_config.speed = speed;
    rgb_matrix_set_speed(speed);
    
    // 自动保存配置
    kob_rgb_save_config();
    
    ESP_LOGI(TAG, "RGB matrix speed set to: %d", speed);
    return ESP_OK;
}

// 下一个灯效模式
esp_err_t kob_rgb_matrix_next_mode(void)
{
    uint16_t next_mode;
    uint16_t max_mode = RGB_MATRIX_EFFECT_MAX - 1;
    
    // 如果当前是Windows Lighting模式，切换到第一个标准模式
    if (g_led_effect_config.mode == RGB_MODE_WINDOWS_LIGHTING) {
        next_mode = 1;
    } else {
        // 标准模式的递增逻辑
        next_mode = g_led_effect_config.mode + 1;
        if (next_mode > max_mode) {
            // 如果已经是最后一个标准模式，切换到Windows Lighting模式
            next_mode = RGB_MODE_WINDOWS_LIGHTING;
        }
    }
    
    return kob_rgb_matrix_set_mode(next_mode);
}

// 上一个灯效模式
esp_err_t kob_rgb_matrix_prev_mode(void)
{
    uint16_t prev_mode;
    uint16_t max_mode = RGB_MATRIX_EFFECT_MAX - 1;
    
    // 如果当前是Windows Lighting模式，切换到最后一个标准模式
    if (g_led_effect_config.mode == RGB_MODE_WINDOWS_LIGHTING) {
        prev_mode = max_mode;
    } else {
        // 标准模式的递减逻辑
        prev_mode = g_led_effect_config.mode - 1;
        if (prev_mode < 1) {
            // 如果已经是第一个标准模式，切换到Windows Lighting模式
            prev_mode = RGB_MODE_WINDOWS_LIGHTING;
        }
    }
    
    return kob_rgb_matrix_set_mode(prev_mode);
}

// OLED菜单动作函数实现

// 增加色调
esp_err_t kob_rgb_matrix_increase_hue(void)
{
    uint8_t new_hue = g_led_effect_config.hue + 10;
    return kob_rgb_matrix_set_hsv(new_hue, g_led_effect_config.sat, g_led_effect_config.val);
}

// 减少色调
esp_err_t kob_rgb_matrix_decrease_hue(void)
{
    uint8_t new_hue = g_led_effect_config.hue - 10;
    return kob_rgb_matrix_set_hsv(new_hue, g_led_effect_config.sat, g_led_effect_config.val);
}

// 增加饱和度
esp_err_t kob_rgb_matrix_increase_sat(void)
{
    uint8_t new_sat = g_led_effect_config.sat + 10;
    // 正确处理无符号整数上溢
    if (new_sat < g_led_effect_config.sat) new_sat = 255;
    return kob_rgb_matrix_set_hsv(g_led_effect_config.hue, new_sat, g_led_effect_config.val);
}

// 减少饱和度
esp_err_t kob_rgb_matrix_decrease_sat(void)
{
    uint8_t new_sat = g_led_effect_config.sat - 10;
    // 正确处理无符号整数下溢
    if (new_sat > g_led_effect_config.sat) new_sat = 0;
    return kob_rgb_matrix_set_hsv(g_led_effect_config.hue, new_sat, g_led_effect_config.val);
}

// 增加亮度
esp_err_t kob_rgb_matrix_increase_val(void)
{
    uint8_t new_val = g_led_effect_config.val + 10;
    // 正确处理无符号整数上溢
    if (new_val < g_led_effect_config.val) new_val = 255;
    return kob_rgb_matrix_set_hsv(g_led_effect_config.hue, g_led_effect_config.sat, new_val);
}

// 减少亮度
esp_err_t kob_rgb_matrix_decrease_val(void)
{
    uint8_t new_val = g_led_effect_config.val - 10;
    // 正确处理无符号整数下溢
    if (new_val > g_led_effect_config.val) new_val = 0;
    return kob_rgb_matrix_set_hsv(g_led_effect_config.hue, g_led_effect_config.sat, new_val);
}

// 增加速度
esp_err_t kob_rgb_matrix_increase_speed(void)
{
    uint8_t new_speed = g_led_effect_config.speed + 10;
    // 正确处理无符号整数上溢
    if (new_speed < g_led_effect_config.speed) new_speed = 255;
    return kob_rgb_matrix_set_speed(new_speed);
}

// 减少速度
esp_err_t kob_rgb_matrix_decrease_speed(void)
{
    uint8_t new_speed = g_led_effect_config.speed - 10;
    // 正确处理无符号整数下溢
    if (new_speed > g_led_effect_config.speed) new_speed = 0;
    return kob_rgb_matrix_set_speed(new_speed);
}

// 处理键盘事件
extern void process_rgb_matrix(uint8_t row, uint8_t col, bool pressed);
void kob_rgb_process_key_event(uint8_t row, uint8_t col, bool pressed)
{
    // 调用RGB矩阵库的按键处理函数
    process_rgb_matrix(row, col, pressed);
    
    // 这里可以添加额外的按键响应逻辑
    // 例如特定按键组合可以切换灯效或修改参数
}

//函数定义
esp_err_t kob_ws2812b_init(led_strip_handle_t *led_strip)
{
    if (s_led_strip) {
        if (led_strip) {
            *led_strip = s_led_strip;
        }
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << WS2812B_POWER_PIN,
                             .mode = GPIO_MODE_OUTPUT_OD,
                             .pull_down_en = 0,
                             .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812B_DATA_PIN, // The GPIO that connected to the LED strip's data line
        .max_leds = WS2812B_NUM, // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812, // LED strip model
        .flags.invert_out = false, // whether to invert the output signal (useful when your hardware has a level inverter)
    };

    // LED strip backend configuration: SPI
    led_strip_spi_config_t spi_config = {
        .clk_src = SOC_MOD_CLK_XTAL, // different clock source can lead to different power consumption
        .flags.with_dma = true,         // Using DMA can improve performance and help drive more LEDs
        .spi_bus = SPI3_HOST,           // SPI bus ID
    };

    // LED Strip object handle
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &s_led_strip));

    if (led_strip) {
        *led_strip = s_led_strip;
    }
    return ESP_OK;
}

esp_err_t kob_ws2812_enable(bool enable)
{
#if KOB_WS2812_USE_SOFTWARE_POWER_OFF
    // 使用软件控制灯珠亮度为0代替关闭电源
    if (!enable) {
        // 清除所有LED灯珠 - 连续调用两次以增加可靠性
        kob_ws2812_clear();
        // 短暂延时确保数据传输完成
        vTaskDelay(10 / portTICK_PERIOD_MS);
        // 再次清除所有LED灯珠
        kob_ws2812_clear();
    }
    // 不实际控制电源引脚，保持电源开启但通过软件控制灯珠关闭
#else
    // 原有逻辑：直接控制电源引脚
    if (!enable) {
        gpio_hold_dis(WS2812B_POWER_PIN);
    }
    gpio_set_level(WS2812B_POWER_PIN, !enable);
    /*!< Make output stable in light sleep */
    if (enable) {
        gpio_hold_en(WS2812B_POWER_PIN);
    }
#endif
    s_led_enable = enable;
    
    // 更新配置结构体中的启用状态
    g_led_effect_config.enabled = enable;
    
    // 自动保存配置
    kob_rgb_save_config();
    
    return ESP_OK;
}

esp_err_t kob_ws2812_clear(void)
{
    if (!s_led_strip) {
        ESP_LOGE(TAG, "s_led_strip is NULL in kob_ws2812_clear");
        return ESP_FAIL;
    }
    
    // 添加错误处理，防止SPI传输失败导致崩溃
    esp_err_t err = led_strip_clear(s_led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear LED strip: %s", esp_err_to_name(err));
    }
    return err;
}


bool kob_ws2812_is_enable(void)
{
    return s_led_enable;
}

esp_err_t kob_rgb_matrix_init(void)
{
    if (!s_led_strip) {
        if (kob_ws2812b_init(NULL) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize WS2812B");
            return ESP_FAIL;
        }
    }
    
    if (s_led_strip) {
        rgb_matrix_driver_init(s_led_strip, WS2812B_NUM);
        rgb_matrix_init();
        
        // 初始化Windows Lighting的互斥锁
        kob_windows_lighting_init();
        
        // 添加错误处理
        esp_err_t err = led_strip_clear(s_led_strip);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to clear LED strip: %s", esp_err_to_name(err));
            // 即使清除失败，也继续初始化，因为可能是临时性问题
        }
        
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get LED strip handle");
        return ESP_FAIL;
    }
}

static void app_led_task(void *arg)
{
    /*!< Init LED and clear WS2812's status */
    esp_err_t err = kob_rgb_matrix_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RGB matrix: %s", esp_err_to_name(err));
    }

    // 等待NVS管理器设置完成（最多等待5秒）
    int wait_count = 0;
    const int max_wait_count = 50; // 5秒
    while (g_unified_nvs_manager == NULL && wait_count < max_wait_count) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        wait_count++;
    }
    
    if (g_unified_nvs_manager == NULL) {
        ESP_LOGW(TAG, "NVS manager not set after waiting, using default configuration");
    } else {
        // 从NVS加载配置
        kob_rgb_load_config();
    }
    
    // 设置灯效模式
    if (g_led_effect_config.mode == RGB_MODE_WINDOWS_LIGHTING) {
        ESP_LOGI(TAG, "RGB matrix initialized in Windows Lighting mode");
        // 在Windows Lighting模式下也需要设置HSV和速度值
        rgb_matrix_sethsv(g_led_effect_config.hue, g_led_effect_config.sat, g_led_effect_config.val);
        rgb_matrix_set_speed(g_led_effect_config.speed);
    } else {
        rgb_matrix_mode(g_led_effect_config.mode);
        
        // 设置HSV值
        rgb_matrix_sethsv(g_led_effect_config.hue, g_led_effect_config.sat, g_led_effect_config.val);
        
        // 设置速度
        rgb_matrix_set_speed(g_led_effect_config.speed);
    }
    
    while (1)
    {
        if (kob_ws2812_is_enable())
        {
            if (g_led_effect_config.mode == RGB_MODE_WINDOWS_LIGHTING) {
                // 在Windows Lighting模式下调用特定的更新函数
                windows_lighting_update();
            } else {
                // 其他模式下调用标准的RGB矩阵任务
                rgb_matrix_task();
            }
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void led_task(void)
{
    // 增加任务优先级从3到5，以确保及时处理LED更新请求
    xTaskCreate(app_led_task, "app_led_task", 4 * 1024, NULL, 5, NULL);
}