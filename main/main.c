#include "spi_scanner.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keyboard_led.h"
#include "nvs_flash.h"



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
    appLedStart();
    xTaskCreate(test_spi_task, "test_spi_task", 4096, NULL, 5, NULL);
}
