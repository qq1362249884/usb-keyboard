#include "nvs_manager_base.h"

NvsManagerBase::NvsManagerBase(const char* namespace_name) 
    : namespace_name(namespace_name), nvs_handle(0), is_initialized(false) {
}

NvsManagerBase::~NvsManagerBase() {
    if (is_initialized) {
        close_nvs();
    }
}

esp_err_t NvsManagerBase::init() {
    if (is_initialized) {
        return ESP_OK;
    }
    
    esp_err_t err = init_nvs_flash();
    if (err != ESP_OK) {
        ESP_LOGE("NVS_BASE", "Failed to initialize NVS flash: %s", get_error_string(err));
        return err;
    }
    
    err = open_nvs(true); // 以只读方式打开进行初始化检查
    if (err != ESP_OK) {
        ESP_LOGE("NVS_BASE", "Failed to open NVS namespace '%s': %s", namespace_name, get_error_string(err));
        return err;
    }
    
    close_nvs();
    is_initialized = true;
    
    ESP_LOGI("NVS_BASE", "NVS manager initialized successfully for namespace '%s'", namespace_name);
    return ESP_OK;
}

esp_err_t NvsManagerBase::save(const char* key, const void* data, size_t size) {
    if (!is_initialized) {
        esp_err_t err = init();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    esp_err_t err = open_nvs(false); // 读写模式
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_set_blob(nvs_handle, key, data, size);
    if (err != ESP_OK) {
        ESP_LOGE("NVS_BASE", "Failed to save data for key '%s': %s", key, get_error_string(err));
        close_nvs();
        return err;
    }
    
    err = commit();
    close_nvs();
    
    if (err == ESP_OK) {
        ESP_LOGI("NVS_BASE", "Data saved successfully for key '%s'", key);
    }
    
    return err;
}

esp_err_t NvsManagerBase::load(const char* key, void* data, size_t size) {
    if (!is_initialized) {
        esp_err_t err = init();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    esp_err_t err = open_nvs(true); // 只读模式
    if (err != ESP_OK) {
        return err;
    }
    
    size_t actual_size = size;
    err = nvs_get_blob(nvs_handle, key, data, &actual_size);
    close_nvs();
    
    if (err == ESP_OK) {
        if (actual_size != size) {
            ESP_LOGW("NVS_BASE", "Size mismatch for key '%s': expected %d, got %d", key, size, actual_size);
        }
        ESP_LOGI("NVS_BASE", "Data loaded successfully for key '%s'", key);
    } else {
        ESP_LOGE("NVS_BASE", "Failed to load data for key '%s': %s", key, get_error_string(err));
    }
    
    return err;
}

esp_err_t NvsManagerBase::erase(const char* key) {
    if (!is_initialized) {
        esp_err_t err = init();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    esp_err_t err = open_nvs(false); // 读写模式
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_erase_key(nvs_handle, key);
    if (err != ESP_OK) {
        ESP_LOGE("NVS_BASE", "Failed to erase key '%s': %s", key, get_error_string(err));
        close_nvs();
        return err;
    }
    
    err = commit();
    close_nvs();
    
    if (err == ESP_OK) {
        ESP_LOGI("NVS_BASE", "Key '%s' erased successfully", key);
    }
    
    return err;
}

bool NvsManagerBase::exists(const char* key) {
    if (!is_initialized) {
        esp_err_t err = init();
        if (err != ESP_OK) {
            return false;
        }
    }
    
    esp_err_t err = open_nvs(true); // 只读模式
    if (err != ESP_OK) {
        return false;
    }
    
    size_t size;
    err = nvs_get_blob(nvs_handle, key, NULL, &size);
    close_nvs();
    
    return (err == ESP_OK);
}

esp_err_t NvsManagerBase::commit() {
    esp_err_t err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS_BASE", "Failed to commit changes: %s", get_error_string(err));
    }
    return err;
}

const char* NvsManagerBase::get_error_string(esp_err_t err) {
    switch (err) {
        case ESP_OK: return "Success";
        case ESP_ERR_NVS_NOT_FOUND: return "NVS entry not found";
        case ESP_ERR_NVS_TYPE_MISMATCH: return "NVS type mismatch";
        case ESP_ERR_NVS_READ_ONLY: return "NVS is read only";
        case ESP_ERR_NVS_NOT_ENOUGH_SPACE: return "Not enough space in NVS";
        case ESP_ERR_NVS_INVALID_NAME: return "Invalid NVS name";
        case ESP_ERR_NVS_INVALID_HANDLE: return "Invalid NVS handle";
        case ESP_ERR_NVS_REMOVE_FAILED: return "Failed to remove NVS entry";
        case ESP_ERR_NVS_KEY_TOO_LONG: return "NVS key too long";
        case ESP_ERR_NO_MEM: return "Out of memory";
        default: return "Unknown error";
    }
}

esp_err_t NvsManagerBase::open_nvs(bool read_only) {
    nvs_open_mode_t mode = read_only ? NVS_READONLY : NVS_READWRITE;
    esp_err_t err = nvs_open(namespace_name, mode, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS_BASE", "Failed to open NVS namespace '%s': %s", namespace_name, get_error_string(err));
    }
    return err;
}

void NvsManagerBase::close_nvs() {
    if (nvs_handle != 0) {
        nvs_close(nvs_handle);
        nvs_handle = 0;
    }
}

esp_err_t NvsManagerBase::init_nvs_flash() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW("NVS_BASE", "NVS partition needs to be erased, erasing...");
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            ESP_LOGE("NVS_BASE", "Failed to erase NVS flash: %s", get_error_string(err));
            return err;
        }
        err = nvs_flash_init();
    }
    
    if (err != ESP_OK) {
        ESP_LOGE("NVS_BASE", "Failed to initialize NVS flash: %s", get_error_string(err));
    }
    
    return err;
}