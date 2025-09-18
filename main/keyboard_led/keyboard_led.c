#include "keyboard_led.h"

#include "rgb_matrix_nvs.h"
#include "nvs_manager/nvs_manager.h"
#include <inttypes.h>

//全局变量定义
static const char *TAG = "app_led";
static led_strip_handle_t s_led_strip = NULL;
static bool s_led_enable = false;

//结构体定义
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
    return ESP_OK;
}

esp_err_t kob_ws2812_clear(void)
{
    return led_strip_clear(s_led_strip);
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
        led_strip_clear(s_led_strip);
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

    // 不要默认开启LED，等待init_app.c中的配置应用 - 解决重启闪烁问题
    // kob_ws2812_enable(true);

    uint16_t index = 1; // 默认值
    uint32_t stored_mode = 0;
    
    // 创建并初始化NVS管理器
    NvsBaseManager_t* nvs_manager = nvs_base_create("keyboard_led");
    if (nvs_manager) {
        if (nvs_base_init(nvs_manager) == ESP_OK && nvs_base_open(nvs_manager, true) == ESP_OK) {
            // 检查键是否存在且读取成功
            if (nvs_base_exists(nvs_manager, "rgb_matrix_mode")) {
                if (nvs_base_load_u32(nvs_manager, "rgb_matrix_mode", &stored_mode) == ESP_OK) {
                    index = (uint16_t)stored_mode;
                    ESP_LOGI(TAG, "Loaded RGB matrix mode from NVS: %d", index);
                }
            } else {
                ESP_LOGI(TAG, "RGB matrix mode not found in NVS, using default: %d", index);
            }
            
            nvs_base_close(nvs_manager);
        }
        nvs_base_destroy(nvs_manager);
    }
    
    // 确保索引有效
    uint16_t max_mode = RGB_MATRIX_EFFECT_MAX - 1; // 假设MAX是包含上限的，所以减1
    if (index < 1 || index > max_mode) {
        index = 1; // 超出范围时使用默认值
        ESP_LOGW(TAG, "Invalid mode index %" PRIu32 ", using default: %d", stored_mode, index);
    }
    
    rgb_matrix_mode(index);
    
    while (1)
    {

        if (kob_ws2812_is_enable())
        {
            rgb_matrix_task();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void led_task(void)
{
    xTaskCreate(app_led_task, "app_led_task", 4 * 1024, NULL, 3, NULL);
}