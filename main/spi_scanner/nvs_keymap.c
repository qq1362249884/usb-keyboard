#include "nvs_keymap.h"
#include "c_wrapper.h"
#include "spi_scanner.h"
#include "esp_log.h"

static const char *TAG = "NVS_KEYMAP";

// 默认按键映射
static const uint16_t default_keymaps[][NUM_KEYS] = {
    [0] ={  KC_ESC , KC_KP_SLASH, KC_KP_ASTERISK, KC_KP_MINUS, 
            KC_KP_7, KC_KP_8, KC_KP_9, KC_KP_PLUS, 
            KC_KP_4, KC_KP_5, KC_KP_6, 
            KC_KP_1, KC_KP_2, KC_KP_3, 
            KC_KP_0, KC_KP_DOT, KC_KP_ENTER},
    [1] ={  KC_ESC , KC_KP_SLASH, KC_KP_ASTERISK, KC_BACKSPACE, 
            KC_Q, KC_W, KC_E, KC_KP_PLUS, 
            KC_A, KC_S, KC_D, 
            KC_KP_1, KC_KP_2, KC_KP_3, 
            KC_KP_0, KC_KP_DOT, KC_KP_ENTER}
};

// NVS存储键名前缀
#define KEYMAP_NVS_KEY_PREFIX "keymap_"

// 全局NVS管理器句柄
static KeymapNvsManager_t* g_nvs_manager = NULL;

// 运行时按键映射（可通过NVS修改）
uint16_t keymaps[2][NUM_KEYS] = {0};

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
    g_nvs_manager = keymap_nvs_manager_create("keymaps", KEYMAP_NVS_KEY_PREFIX, NUM_KEYS, 2);
    if (!g_nvs_manager) {
        ESP_LOGE(TAG, "Failed to create NVS manager");
        return ESP_FAIL;
    }
    
    // 初始化管理器
    esp_err_t err = keymap_nvs_manager_init(g_nvs_manager, 
                                           &default_keymaps[0][0], 
                                           &keymaps[0][0]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS manager: %s", 
                 keymap_nvs_manager_get_error_string(g_nvs_manager, err));
        keymap_nvs_manager_destroy(g_nvs_manager);
        g_nvs_manager = NULL;
        return err;
    }
    
    ESP_LOGI(TAG, "NVS manager initialized successfully");
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
        ESP_LOGE(TAG, "Failed to save keymap for layer %d: %s", layer,
                 keymap_nvs_manager_get_error_string(g_nvs_manager, err));
    } else {
        ESP_LOGI(TAG, "Saved keymap for layer %d successfully", layer);
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
        ESP_LOGE(TAG, "Failed to load keymap for layer %d: %s", layer,
                 keymap_nvs_manager_get_error_string(g_nvs_manager, err));
    } else {
        ESP_LOGI(TAG, "Loaded keymap for layer %d successfully", layer);
        // 复制到输出数组
        memcpy(keymap, &keymaps[layer][0], sizeof(uint16_t) * NUM_KEYS);
    }
    
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
        ESP_LOGE(TAG, "Failed to reset keymap for layer %d: %s", layer,
                 keymap_nvs_manager_get_error_string(g_nvs_manager, err));
    } else {
        ESP_LOGI(TAG, "Reset keymap for layer %d to default successfully", layer);
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
            ESP_LOGE(TAG, "Failed to initialize NVS manager");
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
        ESP_LOGI(TAG, "NVS manager cleaned up");
    }
}