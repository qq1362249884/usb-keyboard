#include "keyboard_led.h"

#include "rgb_matrix_nvs.h"

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
        {11, 12, 13, 14},
        {15, 16,NO_LED,NO_LED}
    },
    {
        // LED Index to Physical Position
        {0, 0}, {9, 0}, {27, 0}, {45, 0},        // 第一行
        {0, 9}, {9, 9}, {27, 9}, {45, 9},    // 第二行
        {0, 27}, {9, 27}, {27, 27},              // 第三行
        {0, 45}, {9, 45}, {27, 45}, {45, 45},    // 第四行
        {0, 63}, {9, 63}                         // 第五行
    },
    {
        // LED Index to Flag
        // 根据需要设置标志位
        4, 4, 4, 4,
        4, 4, 4, 4,
        4, 4, 4,
        4, 4, 4, 4,
        4, 4
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
    if (!enable) {
        gpio_hold_dis(WS2812B_POWER_PIN);
    }
    gpio_set_level(WS2812B_POWER_PIN, !enable);
    /*!< Make output stable in light sleep */
    if (enable) {
        gpio_hold_en(WS2812B_POWER_PIN);
    }
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
        kob_ws2812b_init(NULL);
    }
    rgb_matrix_driver_init(s_led_strip, WS2812B_NUM);
    rgb_matrix_init();
    return ESP_OK;
}

static void appLedTask(void *arg)
{
    /*!< Init LED and clear WS2812's status */
    led_strip_handle_t led_strip = NULL;
    kob_ws2812b_init(&led_strip);
    if (led_strip)
    {
        led_strip_clear(led_strip);
    }
    kob_rgb_matrix_init();

    kob_ws2812_enable(true);

    uint16_t index = rgb_matrix_get_mode();
    ESP_LOGI(TAG, "Current RGB Matrix mode: %d", index);
    if (index == RGB_MATRIX_NONE)
        index = 1;
    rgb_matrix_mode(4); // 设置呼吸灯模式 (索引5)
    // nvs_flush_rgb_matrix(true); // 保存模式到NVS
    ESP_LOGI(TAG, "RGB_MATRIX_EFFECT_MAX: %d", RGB_MATRIX_EFFECT_MAX);

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

void appLedStart(void)
{
    xTaskCreate(appLedTask, "appLedTask", 4 * 1024, NULL, 3, NULL);
}