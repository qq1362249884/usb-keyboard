#include "c_wrapper.h"
#include "keymap_nvs_manager.h"
#include "esp_log.h"

static const char* TAG = "C_WRAPPER";

// C++类的实现细节对外隐藏
struct KeymapNvsManagerWrapper {
    KeymapNvsManager* manager;
};

// C接口实现

KeymapNvsManager_t* keymap_nvs_manager_create(const char* namespace_name, 
                                               const char* key_prefix, 
                                               uint8_t num_keys, 
                                               uint8_t num_layers) {
    KeymapNvsManager_t* wrapper = new KeymapNvsManagerWrapper;
    if (!wrapper) {
        ESP_LOGE(TAG, "Failed to allocate memory for manager wrapper");
        return nullptr;
    }
    
    wrapper->manager = new KeymapNvsManager(namespace_name, key_prefix, num_keys, num_layers);
    if (!wrapper->manager) {
        ESP_LOGE(TAG, "Failed to allocate memory for manager implementation");
        delete wrapper;
        return nullptr;
    }
    
    ESP_LOGI(TAG, "NVS manager created successfully");
    return wrapper;
}

void keymap_nvs_manager_destroy(KeymapNvsManager_t* manager) {
    if (manager) {
        if (manager->manager) {
            delete manager->manager;
        }
        delete manager;
        ESP_LOGI(TAG, "NVS manager destroyed");
    }
}

esp_err_t keymap_nvs_manager_init(KeymapNvsManager_t* manager, 
                                   const uint16_t* default_keymaps, 
                                   uint16_t* keymaps) {
    if (!manager || !manager->manager) {
        ESP_LOGE(TAG, "Invalid manager handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    return manager->manager->init_with_keymaps(default_keymaps, keymaps);
}

esp_err_t keymap_nvs_manager_save(KeymapNvsManager_t* manager, 
                                   uint8_t layer, 
                                   const uint16_t* keymap) {
    if (!manager || !manager->manager) {
        ESP_LOGE(TAG, "Invalid manager handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    return manager->manager->save_keymap(layer, keymap);
}

esp_err_t keymap_nvs_manager_load(KeymapNvsManager_t* manager, 
                                   uint8_t layer, 
                                   uint16_t* keymap) {
    if (!manager || !manager->manager) {
        ESP_LOGE(TAG, "Invalid manager handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    return manager->manager->load_keymap(layer, keymap);
}

esp_err_t keymap_nvs_manager_reset(KeymapNvsManager_t* manager, uint8_t layer) {
    if (!manager || !manager->manager) {
        ESP_LOGE(TAG, "Invalid manager handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    return manager->manager->reset_keymap_to_default(layer);
}

esp_err_t keymap_nvs_manager_save_all(KeymapNvsManager_t* manager, const uint16_t* keymaps) {
    if (!manager || !manager->manager) {
        ESP_LOGE(TAG, "Invalid manager handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    return manager->manager->save_all_keymaps(keymaps);
}

esp_err_t keymap_nvs_manager_load_all(KeymapNvsManager_t* manager, uint16_t* keymaps) {
    if (!manager || !manager->manager) {
        ESP_LOGE(TAG, "Invalid manager handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    return manager->manager->load_all_keymaps(keymaps);
}

int keymap_nvs_manager_exists(KeymapNvsManager_t* manager, uint8_t layer) {
    if (!manager || !manager->manager) {
        ESP_LOGE(TAG, "Invalid manager handle");
        return 0;
    }
    
    return manager->manager->keymap_exists(layer) ? 1 : 0;
}

void keymap_nvs_manager_test_config(KeymapNvsManager_t* manager, uint16_t* keymaps) {
    if (!manager || !manager->manager) {
        ESP_LOGE(TAG, "Invalid manager handle");
        return;
    }
    
    manager->manager->test_keymap_config(keymaps);
}

const char* keymap_nvs_manager_get_error_string(KeymapNvsManager_t* manager, esp_err_t err) {
    if (!manager || !manager->manager) {
        return "Invalid manager handle";
    }
    
    return manager->manager->get_error_string(err);
}