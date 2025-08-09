#include "keymap_nvs_manager.h"
#include "string.h"
#include "keycodes.h"

KeymapNvsManager::KeymapNvsManager(const char* namespace_name, const char* key_prefix, 
                                   uint8_t num_keys, uint8_t num_layers)
    : NvsManagerBase(namespace_name), key_prefix(key_prefix), num_keys(num_keys), 
      num_layers(num_layers), default_keymaps(nullptr) {
}

KeymapNvsManager::~KeymapNvsManager() {
}

esp_err_t KeymapNvsManager::init_with_keymaps(const uint16_t* default_keymaps, uint16_t* keymaps) {
    this->default_keymaps = default_keymaps;
    
    esp_err_t err = NvsManagerBase::init();
    if (err != ESP_OK) {
        return err;
    }
    
    // 加载所有层的按键映射
    err = load_all_keymaps(keymaps);
    if (err != ESP_OK) {
        ESP_LOGW("KEYMAP_NVS", "Failed to load all keymaps, some layers may use default values");
        // 即使加载失败也继续，因为部分层可能成功加载
    }
    
    return ESP_OK;
}

esp_err_t KeymapNvsManager::save_keymap(uint8_t layer, const uint16_t* keymap) {
    if (!is_valid_layer(layer)) {
        ESP_LOGE("KEYMAP_NVS", "Invalid layer index: %d", layer);
        return ESP_ERR_INVALID_ARG;
    }
    
    char key[32];
    generate_key_name(layer, key, sizeof(key));
    
    esp_err_t err = save(key, keymap, sizeof(uint16_t) * num_keys);
    if (err == ESP_OK) {
        ESP_LOGI("KEYMAP_NVS", "Saved keymap for layer %d successfully", layer);
    }
    
    return err;
}

esp_err_t KeymapNvsManager::load_keymap(uint8_t layer, uint16_t* keymap) {
    if (!is_valid_layer(layer)) {
        ESP_LOGE("KEYMAP_NVS", "Invalid layer index: %d", layer);
        return ESP_ERR_INVALID_ARG;
    }
    
    char key[32];
    generate_key_name(layer, key, sizeof(key));
    
    esp_err_t err = load(key, keymap, sizeof(uint16_t) * num_keys);
    if (err == ESP_OK) {
        ESP_LOGI("KEYMAP_NVS", "Loaded keymap for layer %d successfully", layer);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        // 如果找不到，使用默认值
        if (default_keymaps != nullptr) {
            ESP_LOGW("KEYMAP_NVS", "Keymap for layer %d not found, using default", layer);
            memcpy(keymap, &default_keymaps[layer * num_keys], sizeof(uint16_t) * num_keys);
            return ESP_OK;
        } else {
            ESP_LOGE("KEYMAP_NVS", "Keymap for layer %d not found and no default available", layer);
        }
    }
    
    return err;
}

esp_err_t KeymapNvsManager::reset_keymap_to_default(uint8_t layer) {
    if (!is_valid_layer(layer)) {
        ESP_LOGE("KEYMAP_NVS", "Invalid layer index: %d", layer);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (default_keymaps == nullptr) {
        ESP_LOGE("KEYMAP_NVS", "Default keymaps not available");
        return ESP_ERR_NO_MEM;
    }
    
    // 复制默认映射
    uint16_t default_keymap[num_keys];
    memcpy(default_keymap, &default_keymaps[layer * num_keys], sizeof(uint16_t) * num_keys);
    
    // 保存默认映射
    esp_err_t err = save_keymap(layer, default_keymap);
    if (err == ESP_OK) {
        ESP_LOGI("KEYMAP_NVS", "Reset keymap for layer %d to default successfully", layer);
    }
    
    return err;
}

esp_err_t KeymapNvsManager::load_all_keymaps(uint16_t* keymaps) {
    esp_err_t overall_err = ESP_OK;
    
    for (uint8_t layer = 0; layer < num_layers; layer++) {
        esp_err_t err = load_keymap(layer, &keymaps[layer * num_keys]);
        if (err != ESP_OK) {
            ESP_LOGW("KEYMAP_NVS", "Failed to load keymap for layer %d: %s", layer, get_error_string(err));
            overall_err = err; // 记录最后一个错误
        }
    }
    
    return overall_err;
}

esp_err_t KeymapNvsManager::save_all_keymaps(const uint16_t* keymaps) {
    esp_err_t overall_err = ESP_OK;
    
    for (uint8_t layer = 0; layer < num_layers; layer++) {
        esp_err_t err = save_keymap(layer, &keymaps[layer * num_keys]);
        if (err != ESP_OK) {
            ESP_LOGE("KEYMAP_NVS", "Failed to save keymap for layer %d: %s", layer, get_error_string(err));
            overall_err = err; // 记录最后一个错误
        }
    }
    
    return overall_err;
}

bool KeymapNvsManager::keymap_exists(uint8_t layer) {
    if (!is_valid_layer(layer)) {
        return false;
    }
    
    char key[32];
    generate_key_name(layer, key, sizeof(key));
    
    return exists(key);
}

void KeymapNvsManager::test_keymap_config(uint16_t* keymaps) {
    ESP_LOGI("KEYMAP_NVS", "Starting keymap configuration test");
    
    // 测试修改层1的按键映射
    uint8_t test_layer = 1;
    uint16_t new_keymap[num_keys];
    
    // 复制当前映射
    memcpy(new_keymap, &keymaps[test_layer * num_keys], sizeof(uint16_t) * num_keys);
    
    // 修改几个按键（这里使用一些示例键码）
    new_keymap[0] = KC_0;      // U键
    new_keymap[1] = KC_1;      // I键
    new_keymap[2] = KC_2;      // O键
    
    ESP_LOGI("KEYMAP_NVS", "Modified keymap for layer %d", test_layer);
    
    // 保存新映射
    esp_err_t err = save_keymap(test_layer, new_keymap);
    if (err == ESP_OK) {
        ESP_LOGI("KEYMAP_NVS", "Saved new keymap successfully");
        
        // 应用新映射到运行时数组
        memcpy(&keymaps[test_layer * num_keys], new_keymap, sizeof(uint16_t) * num_keys);
        ESP_LOGI("KEYMAP_NVS", "Applied new keymap to runtime array:");
        for (int i = 0; i < 3; i++) {
            ESP_LOGI("KEYMAP_NVS", "Key %d: 0x%04X", i, keymaps[test_layer * num_keys + i]);
        }
        
        // 重新加载映射（验证）
        uint16_t loaded_keymap[num_keys];
        err = load_keymap(test_layer, loaded_keymap);
        if (err == ESP_OK) {
            ESP_LOGI("KEYMAP_NVS", "Loaded keymap successfully");
            // 验证加载的映射是否正确
            if (memcmp(loaded_keymap, new_keymap, sizeof(uint16_t) * num_keys) == 0) {
                ESP_LOGI("KEYMAP_NVS", "Keymap verification passed");
                // 确保运行时数组与加载的映射一致
                memcpy(&keymaps[test_layer * num_keys], loaded_keymap, sizeof(uint16_t) * num_keys);
            } else {
                ESP_LOGW("KEYMAP_NVS", "Keymap verification failed");
            }
        }
    }
    
    // 测试重置功能（暂时注释，以便观察修改效果）
    /*
    err = reset_keymap_to_default(test_layer);
    if (err == ESP_OK) {
        ESP_LOGI("KEYMAP_NVS", "Reset keymap to default successfully");
    }
    */
    
    ESP_LOGI("KEYMAP_NVS", "Keymap configuration test completed");
}

void KeymapNvsManager::generate_key_name(uint8_t layer, char* key_buffer, size_t buffer_size) {
    snprintf(key_buffer, buffer_size, "%s%d", key_prefix, layer);
}

bool KeymapNvsManager::is_valid_layer(uint8_t layer) const {
    return layer < num_layers;
}