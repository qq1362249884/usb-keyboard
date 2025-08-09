#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keyboard_led.h"
#include "nvs_flash.h"
#include "joystick.h"
#include "spi_scanner.h"
#include "nvs_keymap.h"
#include "keymap_test.h"


void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    spi_hid_init();
    led_task();
    //joystick_task();
    spi_scanner_keyboard_task();

    // 启动按键映射测试（实际应用中可以根据需要选择是否启用）
    start_keymap_test();
}
