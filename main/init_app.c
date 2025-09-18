/*
 * 初始化应用示例
 * 展示如何使用初始化管理器协调各个硬件模块的初始化顺序
 */
#include "init_manager/init_manager.h"
#include "nvs_flash.h"
#include "keyboard_led.h"
#include "oled_menu_display.h"
#include "spi_scanner.h"
#include "wifi_app/wifi_app.h"
#include "nvs_manager/menu_nvs_manager.h"
#include "esp_log.h"

// 全局菜单NVS管理器句柄
static MenuNvsManager_t* g_menu_nvs_manager = NULL;

// 日志标签
static const char *TAG = "INIT_APP";

/**
 * @brief NVS模块初始化函数
 */
static esp_err_t init_nvs(void) {
    ESP_LOGI(TAG, "Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs to be erased, performing erase...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief NVS模块配置应用函数
 */
static esp_err_t apply_nvs_config(void) {
    ESP_LOGI(TAG, "Applying NVS configuration...");
    
    // 创建菜单NVS管理器实例
    g_menu_nvs_manager = menu_nvs_manager_create(NULL, 0, false);
    if (!g_menu_nvs_manager) {
        ESP_LOGE(TAG, "Failed to create menu NVS manager");
        return ESP_FAIL;
    }
    
    // 初始化NVS管理器
    return menu_nvs_manager_init(g_menu_nvs_manager);
}

/**
 * @brief WS2812 LED模块初始化函数
 */
static esp_err_t init_ws2812(void) {
    ESP_LOGI(TAG, "Initializing WS2812...");
    // 创建LED任务（会初始化WS2812硬件）
    led_task();
    return ESP_OK;
}

/**
 * @brief WS2812 LED模块配置应用函数
 */
static esp_err_t apply_ws2812_config(void) {
    ESP_LOGI(TAG, "Applying WS2812 configuration...");
    
    if (!g_menu_nvs_manager) {
        ESP_LOGE(TAG, "Menu NVS manager not initialized");
        return ESP_FAIL;
    }
    
    // 从NVS加载WS2812状态
    bool ws2812_state = false;
    esp_err_t err = menu_nvs_manager_load_ws2812_state(g_menu_nvs_manager, &ws2812_state);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to load WS2812 state: %s", esp_err_to_name(err));
        return err;
    }
    
    // 应用WS2812状态
    ESP_LOGI(TAG, "Setting WS2812 state to: %d", ws2812_state);
    err = kob_ws2812_enable(ws2812_state);
    
    // 添加重试机制，确保状态正确应用
    if (err == ESP_OK) {
        int retry_count = 0;
        const int max_retries = 3;
        
        while (retry_count < max_retries) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            bool current_state = kob_ws2812_is_enable();
            
            if (current_state == ws2812_state) {
                ESP_LOGI(TAG, "WS2812 state verified successfully");
                break;
            }
            
            ESP_LOGW(TAG, "WS2812 state mismatch, retrying (%d/%d)...", 
                     retry_count + 1, max_retries);
            err = kob_ws2812_enable(ws2812_state);
            retry_count++;
        }
        
        if (retry_count == max_retries) {
            ESP_LOGE(TAG, "Failed to set WS2812 state after %d attempts", max_retries);
            return ESP_FAIL;
        }
    }
    
    return err;
}

/**
 * @brief OLED显示模块初始化函数
 */
static esp_err_t init_oled(void) {
    ESP_LOGI(TAG, "Initializing OLED display...");
    // OLED显示的初始化会在oled_menu_example_start内部完成
    return ESP_OK;
}

/**
 * @brief OLED显示模块配置应用函数
 */
static esp_err_t apply_oled_config(void) {
    ESP_LOGI(TAG, "Applying OLED configuration...");
    
    // 启动OLED菜单系统
    oled_menu_example_start();
    
    // 从NVS加载图层配置
    if (g_menu_nvs_manager) {
        uint8_t layer = 0;
        esp_err_t err = menu_nvs_manager_load_current_layer(g_menu_nvs_manager, &layer);
        if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to load current layer: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Current keymap layer: %d", layer);
            // 这里可以设置当前图层（如果有相应的API）
        }
    }
    
    return ESP_OK;
}

/**
 * @brief 键盘扫描模块初始化函数
 */
static esp_err_t init_keyboard(void) {
    ESP_LOGI(TAG, "Initializing keyboard scanner...");
    // 创建键盘扫描任务
    spi_scanner_keyboard_task();
    return ESP_OK;
}

/**
 * @brief 键盘扫描模块配置应用函数
 */
static esp_err_t apply_keyboard_config(void) {
    ESP_LOGI(TAG, "Applying keyboard configuration...");
    // 键盘模块可能不需要额外的配置应用
    return ESP_OK;
}

/**
 * @brief WiFi模块初始化函数
 */
static esp_err_t init_wifi(void) {
    ESP_LOGI(TAG, "Initializing WiFi...");
    // 创建WiFi任务
    wifi_task();
    return ESP_OK;
}

/**
 * @brief WiFi模块配置应用函数
 */
static esp_err_t apply_wifi_config(void) {
    ESP_LOGI(TAG, "Applying WiFi configuration...");
    // WiFi配置可能在wifi_task内部处理
    return ESP_OK;
}

/**
 * @brief 应用程序初始化函数
 * 使用初始化管理器协调所有模块的初始化顺序
 */
esp_err_t app_init(void) {
    ESP_LOGI(TAG, "Starting application initialization...");
    
    // 初始化管理器
    esp_err_t ret = init_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize init manager: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 定义模块初始化描述符
    module_init_desc_t nvs_desc = {
        .module_id = MODULE_NVS,
        .init_func = init_nvs,
        .apply_config_func = apply_nvs_config,
        .dependencies = {}, // NVS模块没有依赖
        .dependency_count = 0,
        .state = INIT_STATE_UNINITIALIZED,
        .ready_sem = NULL
    };
    
    module_init_desc_t ws2812_desc = {
        .module_id = MODULE_WS2812,
        .init_func = init_ws2812,
        .apply_config_func = apply_ws2812_config,
        .dependencies = {MODULE_NVS}, // WS2812依赖NVS
        .dependency_count = 1,
        .state = INIT_STATE_UNINITIALIZED,
        .ready_sem = NULL
    };
    
    module_init_desc_t oled_desc = {
        .module_id = MODULE_OLED,
        .init_func = init_oled,
        .apply_config_func = apply_oled_config,
        .dependencies = {MODULE_NVS}, // OLED依赖NVS
        .dependency_count = 1,
        .state = INIT_STATE_UNINITIALIZED,
        .ready_sem = NULL
    };
    
    module_init_desc_t keyboard_desc = {
        .module_id = MODULE_KEYBOARD,
        .init_func = init_keyboard,
        .apply_config_func = apply_keyboard_config,
        .dependencies = {}, // 键盘模块没有依赖
        .dependency_count = 0,
        .state = INIT_STATE_UNINITIALIZED,
        .ready_sem = NULL
    };
    
    module_init_desc_t wifi_desc = {
        .module_id = MODULE_WIFI,
        .init_func = init_wifi,
        .apply_config_func = apply_wifi_config,
        .dependencies = {MODULE_NVS}, // WiFi依赖NVS
        .dependency_count = 1,
        .state = INIT_STATE_UNINITIALIZED,
        .ready_sem = NULL
    };
    
    // 注册所有模块
    ret = init_manager_register_module(&nvs_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register NVS module: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_manager_register_module(&ws2812_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WS2812 module: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_manager_register_module(&oled_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register OLED module: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_manager_register_module(&keyboard_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register keyboard module: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_manager_register_module(&wifi_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi module: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 启动所有模块的初始化过程
    ret = init_manager_start_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start initialization: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 等待关键模块初始化完成
    ESP_LOGI(TAG, "Waiting for critical modules to initialize...");
    
    // 等待NVS模块初始化完成
    ret = init_manager_wait_for_module(MODULE_NVS, 5000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS module initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 应用NVS配置（因为其他模块的配置依赖于NVS）
    ret = init_manager_apply_module_config(MODULE_NVS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply NVS configuration: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 等待WS2812模块初始化完成
    ret = init_manager_wait_for_module(MODULE_WS2812, 5000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 module initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 应用WS2812配置（这是关键的修复点，确保在硬件初始化后再应用配置）
    ret = init_manager_apply_module_config(MODULE_WS2812);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply WS2812 configuration: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 应用其他模块的配置
    ret = init_manager_apply_module_config(MODULE_OLED);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply OLED configuration: %s", esp_err_to_name(ret));
    }
    
    ret = init_manager_apply_module_config(MODULE_KEYBOARD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply keyboard configuration: %s", esp_err_to_name(ret));
    }
    
    ret = init_manager_apply_module_config(MODULE_WIFI);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply WiFi configuration: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "Application initialization completed successfully");
    return ESP_OK;
}