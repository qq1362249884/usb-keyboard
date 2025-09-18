#include "keymap_manager.h"

#ifdef __cplusplus
extern "C" {
#endif


static const char *TAG_NVS = "NVS_KEYMAP";
static const char *TAG_TEST = "KEYMAP_TEST";

// NVS存储键名前缀
#define KEYMAP_NVS_KEY_PREFIX "keymap_"

// 默认按键映射
// 注意：层0是不可修改的默认映射，层1是可自定义的映射
uint16_t keymaps[2][NUM_KEYS] = {
    // 层0 - 不可修改的默认映射
    [0] ={  KC_ESC , KC_KP_SLASH, KC_KP_ASTERISK, KC_KP_MINUS, 
            KC_KP_7, KC_KP_8, KC_KP_9, KC_KP_PLUS, 
            KC_KP_4, KC_KP_5, KC_KP_6, 
            KC_KP_1, KC_KP_2, KC_KP_3, 
            KC_KP_0, KC_KP_DOT, KC_KP_ENTER},
    // 层1 - 可自定义的映射
    [1] ={}
};

// 全局NVS管理器句柄
static KeymapNvsManager_t* g_nvs_manager = NULL;


/**
 * @brief 初始化NVS并加载按键映射
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t nvs_keymap_init(void) {    
    // 如果NVS管理器已存在，先销毁
    if (g_nvs_manager) {
        keymap_nvs_manager_destroy(g_nvs_manager);
        g_nvs_manager = NULL;
    }
    
    // 创建NVS管理器实例
    g_nvs_manager = keymap_nvs_manager_create("keymaps", KEYMAP_NVS_KEY_PREFIX, NUM_KEYS, 2, &keymaps[0][0]);
    if (!g_nvs_manager) {
        ESP_LOGE(TAG_NVS, "Failed to create NVS manager");
        return ESP_FAIL;
    }
    
    // 初始化管理器
    esp_err_t err = keymap_nvs_manager_init(g_nvs_manager);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Failed to initialize NVS manager");
        keymap_nvs_manager_destroy(g_nvs_manager);
        g_nvs_manager = NULL;
        return err;
    }
    
    // 从NVS加载自定义层(层1)的按键映射数据到运行时数组
    // 这样系统重启后，自定义层的数据会自动加载
    err = keymap_nvs_manager_load(g_nvs_manager, 1, &keymaps[1][0]);
    if (err == ESP_OK) {
        ESP_LOGI(TAG_NVS, "Successfully loaded custom keymap (layer 1) from NVS");
    } else if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG_NVS, "No saved custom keymap found in NVS, using default values for layer 1");
    } else {
        ESP_LOGE(TAG_NVS, "Failed to load custom keymap from NVS: %s", esp_err_to_name(err));
    }
    
    ESP_LOGI(TAG_NVS, "NVS manager initialized successfully");
    return ESP_OK;
}

/**
 * @brief 保存按键映射到NVS
 * @param layer 层索引
 * @param keymap 按键映射数组
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t save_keymap_to_nvs(uint8_t layer, const uint16_t *keymap) {
    if (!g_nvs_manager) {
        esp_err_t err = nvs_keymap_init();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    // 复制到运行时数组
    memcpy(&keymaps[layer][0], keymap, sizeof(uint16_t) * NUM_KEYS);
    
    esp_err_t err = keymap_nvs_manager_save(g_nvs_manager, layer, &keymaps[layer][0]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Failed to save keymap for layer %d", layer);
    } else {
        ESP_LOGI(TAG_NVS, "Saved keymap for layer %d successfully", layer);
    }
    
    return err;
}

/**
 * @brief 从NVS加载按键映射
 * @param layer 层索引
 * @param keymap 用于存储按键映射的数组
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t load_keymap_from_nvs(uint8_t layer, uint16_t *keymap) {
    if (!g_nvs_manager) {
        esp_err_t err = nvs_keymap_init();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    esp_err_t err = keymap_nvs_manager_load(g_nvs_manager, layer, &keymaps[layer][0]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Failed to load keymap for layer %d", layer);
    } else {
        ESP_LOGI(TAG_NVS, "Loaded keymap for layer %d successfully", layer);
    }
    
    // 无论加载是否成功，都将当前映射复制到输出数组
    // 这样即使加载失败，也能返回内存中的默认映射
    memcpy(keymap, &keymaps[layer][0], sizeof(uint16_t) * NUM_KEYS);
    
    return err;
}

/**
 * @brief 将按键映射重置为默认值
 * @param layer 层索引
 * @return ESP_OK 成功
 * @return 其他 失败
 */
esp_err_t reset_keymap_to_default(uint8_t layer) {
    if (!g_nvs_manager) {
        esp_err_t err = nvs_keymap_init();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    esp_err_t err = keymap_nvs_manager_reset(g_nvs_manager, layer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Failed to reset keymap for layer %d", layer);
    } else {
        ESP_LOGI(TAG_NVS, "Reset keymap for layer %d to default successfully", layer);
    }
    
    return err;
}

/**
 * @brief 测试按键映射配置功能
 * 这个函数演示如何修改、保存和加载按键映射
 */
void test_keymap_config(void) {
    if (!g_nvs_manager) {
        esp_err_t err = nvs_keymap_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG_NVS, "Failed to initialize NVS manager");
            return;
        }
    }
    
    // 使用C++的测试函数
    keymap_nvs_manager_test_config(g_nvs_manager, &keymaps[0][0]);
}

/**
 * @brief 清理NVS管理器资源
 * 这个函数应该在程序退出时调用，释放NVS管理器资源
 */
void nvs_keymap_cleanup(void) {
    if (g_nvs_manager) {
        keymap_nvs_manager_destroy(g_nvs_manager);
        g_nvs_manager = NULL;
        ESP_LOGI(TAG_NVS, "NVS manager cleaned up");
    }
}

/**
 * @brief 按键映射测试任务
 * 这个任务演示如何使用NVS功能来修改、保存和加载按键映射
 */
void keymap_test_task(void *pvParameter) {
    ESP_LOGI(TAG_TEST, "Starting keymap test task");

    // 等待系统初始化完成
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // 初始化NVS（如果还没有初始化）
    esp_err_t err = nvs_keymap_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_TEST, "Failed to initialize NVS manager");
        vTaskDelete(NULL);
    }

    // 运行测试函数
    test_keymap_config();

    // 测试持续运行，以便观察效果
    while (1) {
        // 每5秒打印一次当前按键映射
        ESP_LOGI(TAG_TEST, "Current keymap for layer 1 (address: %p):", keymaps[1]);
        for (int i = 0; i < NUM_KEYS; i++) {
            ESP_LOGI(TAG_TEST, "Key %d: 0x%04X", i, keymaps[1][i]);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

/**
 * @brief 启动按键映射测试
 */
void start_keymap_test(void) {
    xTaskCreate(keymap_test_task, "keymap_test_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG_TEST, "Keymap test task created");
}

#ifdef __cplusplus
}
#endif