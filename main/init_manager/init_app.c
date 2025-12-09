/*
 * 初始化应用示例
 * 展示如何使用初始化管理器协调各个硬件模块的初始化顺序
 */
#include "init_manager/init_manager.h"
#include "nvs_flash.h"
#include "keyboard_led.h"
#include "oled_menu_display.h"
#include "spi_scanner.h"
#include "spi_scanner/keymap_manager.h"
#include "wifi_app/wifi_app.h"
#include "nvs_manager/unified_nvs_manager.h"
#include "audio_player/mp3_player.h"
#include "esp_log.h"

// 全局统一NVS管理器句柄
unified_nvs_manager_t* g_unified_nvs_manager = NULL;

// 日志标签
static const char *INIT_APP_TAG = "INIT_APP";

/**
 * @brief NVS模块初始化函数
 */
static esp_err_t init_nvs(void) {
    ESP_LOGI(INIT_APP_TAG, "Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(INIT_APP_TAG, "NVS needs to be erased, performing erase...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief NVS模块配置应用函数
 */
static esp_err_t apply_nvs_config(void) {
    
    // 创建统一NVS管理器实例
    g_unified_nvs_manager = unified_nvs_manager_create_default();
    if (!g_unified_nvs_manager) {
        return ESP_FAIL;
    }
    
    // 初始化NVS管理器
    return unified_nvs_manager_init(g_unified_nvs_manager);
}

/**
 * @brief WS2812 LED模块初始化函数
 */
static esp_err_t init_ws2812(void) {
    // 创建LED任务（会初始化WS2812硬件）
    led_task();
    return ESP_OK;
}

/**
 * @brief WS2812 LED模块配置应用函数
 */
static esp_err_t apply_ws2812_config(void) {
    
    if (!g_unified_nvs_manager) {
        return ESP_FAIL;
    }
    
    // 设置统一NVS管理器实例到键盘LED模块
    kob_rgb_set_nvs_manager(g_unified_nvs_manager);
    
    // 从NVS加载LED配置
    esp_err_t err = kob_rgb_load_config();
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(INIT_APP_TAG, "Failed to load LED configuration: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(INIT_APP_TAG, "LED configuration loaded successfully");
    }
    
    // 从NVS加载WS2812状态
    uint8_t current_layer = 0;
    bool ws2812_state = false;
    err = unified_nvs_load_menu_config(g_unified_nvs_manager, &current_layer, &ws2812_state);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(INIT_APP_TAG, "Failed to load menu config: %s", esp_err_to_name(err));
        return err;
    }
    
    // 应用WS2812状态
    ESP_LOGI(INIT_APP_TAG, "Setting WS2812 state to: %d", ws2812_state);
    err = kob_ws2812_enable(ws2812_state);
    
    // 添加重试机制，确保状态正确应用
    if (err == ESP_OK) {
        int retry_count = 0;
        const int max_retries = 3;
        
        while (retry_count < max_retries) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            bool current_state = kob_ws2812_is_enable();
            
            if (current_state == ws2812_state) {
                ESP_LOGI(INIT_APP_TAG, "WS2812 state verified successfully");
                break;
            }
            
            ESP_LOGW(INIT_APP_TAG, "WS2812 state mismatch, retrying (%d/%d)...", 
                     retry_count + 1, max_retries);
            err = kob_ws2812_enable(ws2812_state);
            retry_count++;
        }
        
        if (retry_count == max_retries) {
            ESP_LOGE(INIT_APP_TAG, "Failed to set WS2812 state after %d attempts", max_retries);
            return ESP_FAIL;
        }
    }
    
    return err;
}

/**
 * @brief OLED显示模块初始化函数
 */
static esp_err_t init_oled(void) {
    // OLED显示的初始化会在oled_menu_example_start内部完成
    return ESP_OK;
}

/**
 * @brief OLED显示模块配置应用函数
 */
static esp_err_t apply_oled_config(void) {
    
    // 设置统一NVS管理器实例到OLED菜单模块
    if (g_unified_nvs_manager) {
        set_unified_nvs_manager(g_unified_nvs_manager);
    }
    
    // 启动OLED菜单系统
    oled_menu_example_start();
    
    // 从NVS加载图层配置
    if (g_unified_nvs_manager) {
        uint8_t layer = 0;
        bool ws2812_state = true;
        esp_err_t err = unified_nvs_load_menu_config(g_unified_nvs_manager, &layer, &ws2812_state);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(INIT_APP_TAG, "Failed to load menu config: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(INIT_APP_TAG, "Current keymap layer: %d", layer);
            // 这里可以设置当前图层（如果有相应的API）
        }
    }
    
    return ESP_OK;
}

/**
 * @brief 键盘扫描模块初始化函数
 */
static esp_err_t init_keyboard(void) {
    // 创建键盘扫描任务
    spi_scanner_keyboard_task();
    return ESP_OK;
}

/**
 * @brief 键盘扫描模块配置应用函数
 */
static esp_err_t apply_keyboard_config(void) {
    
    if (!g_unified_nvs_manager) {
        return ESP_FAIL;
    }
    
    // 设置统一NVS管理器实例到键盘映射模块
    set_nvs_manager(g_unified_nvs_manager);
    
    // 在系统初始化阶段加载键盘映射配置
    esp_err_t err = nvs_keymap_init();
    if (err != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to initialize keyboard mapping: %s", esp_err_to_name(err));
        return err;
    }
    
    return ESP_OK;
}

/**
 * @brief WiFi模块初始化函数
 */
static esp_err_t init_wifi(void) {
    // WiFi的硬件初始化会在wifi_task中完成
    return ESP_OK;
}

/**
 * @brief MP3播放器模块初始化函数
 */
static esp_err_t init_mp3_player(void) {
    // MP3播放器的初始化会在mp3_player_init内部完成
    return ESP_OK;
}

/**
 * @brief MP3播放器模块配置应用函数
 */
static esp_err_t apply_mp3_player_config(void) {
    // MP3播放器不再在系统初始化时自动启动，而是由OLED菜单控制
    // 只做初始化准备，不启动播放任务
    ESP_LOGI(INIT_APP_TAG, "MP3 player initialized, ready to be started by menu");
    return ESP_OK;
}

/**
 * @brief WiFi模块配置应用函数
 */
static esp_err_t apply_wifi_config(void) {
    
    if (!g_unified_nvs_manager) {
        return ESP_FAIL;
    }
    
    // 从NVS读取WiFi启用状态
    bool wifi_enabled = false;
    esp_err_t err = unified_nvs_manager_load(g_unified_nvs_manager, 
                                              NVS_NAMESPACE_WIFI, 
                                              "enabled", 
                                              &wifi_enabled, 
                                              UNIFIED_NVS_TYPE_BOOL, 
                                              NULL);
    
    if (err == ESP_OK) {
        ESP_LOGI(INIT_APP_TAG, "WiFi启用状态读取成功: %s", wifi_enabled ? "启用" : "禁用");
        
        // 如果WiFi启用状态为true，则调用WiFi开关函数启用WiFi
        if (wifi_enabled) {
            err = wifi_station_change(true);
            if (err != ESP_OK) {
                ESP_LOGE(INIT_APP_TAG, "启用WiFi失败: %s", esp_err_to_name(err));
                return err;
            }
            ESP_LOGI(INIT_APP_TAG, "WiFi启用成功");
        } 
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        // 保存默认状态到NVS
        bool default_wifi_enabled = false;
        err = UNIFIED_NVS_SAVE_BOOL(g_unified_nvs_manager, NVS_NAMESPACE_WIFI, "enabled", default_wifi_enabled);
        if (err != ESP_OK) {
            ESP_LOGE(INIT_APP_TAG, "保存默认WiFi状态失败: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(INIT_APP_TAG, "读取WiFi启用状态失败: %s", esp_err_to_name(err));
        return err;
    }
    
    return ESP_OK;
}



/**
 * @brief 应用程序初始化函数
 * 使用初始化管理器协调所有模块的初始化顺序
 */
esp_err_t app_init(void) {
    
    // 初始化管理器
    esp_err_t ret = init_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to initialize init manager: %s", esp_err_to_name(ret));
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

    module_init_desc_t mp3_player_desc = {
        .module_id = MODULE_MP3_PLAYER,
        .init_func = init_mp3_player,
        .apply_config_func = apply_mp3_player_config,
        .dependencies = {}, // MP3播放器没有依赖
        .dependency_count = 0,
        .state = INIT_STATE_UNINITIALIZED,
        .ready_sem = NULL
    };

    
    // 注册所有模块
    ret = init_manager_register_module(&nvs_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to register NVS module: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_manager_register_module(&ws2812_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to register WS2812 module: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_manager_register_module(&oled_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to register OLED module: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_manager_register_module(&keyboard_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to register keyboard module: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_manager_register_module(&wifi_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to register WiFi module: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_manager_register_module(&mp3_player_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to register MP3 player module: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 启动所有模块的初始化过程
    ret = init_manager_start_init();
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to start initialization: %s", esp_err_to_name(ret));
        return ret;
    }
       
    // 等待NVS模块初始化完成
    ret = init_manager_wait_for_module(MODULE_NVS, 5000);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "NVS module initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 应用NVS配置（因为其他模块的配置依赖于NVS）
    ret = init_manager_apply_module_config(MODULE_NVS);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to apply NVS configuration: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 等待WS2812模块初始化完成
    ret = init_manager_wait_for_module(MODULE_WS2812, 5000);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "WS2812 module initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 应用WS2812配置（这是关键的修复点，确保在硬件初始化后再应用配置）
    ret = init_manager_apply_module_config(MODULE_WS2812);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to apply WS2812 configuration: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 应用其他模块的配置
    ret = init_manager_apply_module_config(MODULE_OLED);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to apply OLED configuration: %s", esp_err_to_name(ret));
    }
    
    ret = init_manager_apply_module_config(MODULE_KEYBOARD);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to apply keyboard configuration: %s", esp_err_to_name(ret));
    }
    
    ret = init_manager_apply_module_config(MODULE_WIFI);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to apply WiFi configuration: %s", esp_err_to_name(ret));
    }
    
    ret = init_manager_apply_module_config(MODULE_MP3_PLAYER);
    if (ret != ESP_OK) {
        ESP_LOGE(INIT_APP_TAG, "Failed to apply MP3 player configuration: %s", esp_err_to_name(ret));
    }
    
    return ESP_OK;
}