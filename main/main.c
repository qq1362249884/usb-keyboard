#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "init_app.h"
#include "esp_log.h"
#include "oled_menu_display.h"

static const char *TAG = "MAIN";

extern void app_main(void);

void app_main(void)
{
    // 使用初始化管理器协调所有硬件模块的初始化
    esp_err_t ret = app_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Application initialization failed: %s", esp_err_to_name(ret));
        // 即使初始化失败，也尝试继续运行，以便进行调试
    }

    
    // 应用程序主循环可以在这里添加（如果需要）
    // 但在这个项目中，各个功能已经在各自的任务中实现
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
