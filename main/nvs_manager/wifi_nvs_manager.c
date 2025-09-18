#include "wifi_nvs_manager.h"
#include "nvs_manager.h"
#include <string.h>

// 定义NVS命名空间和键名
#define WIFI_CONFIG_NAMESPACE "wifi_config"
#define WIFI_CONFIG_SSID_KEY "ssid"
#define WIFI_CONFIG_PASS_KEY "password"

// 包装器结构定义
struct WifiNvsManagerWrapper {
    NvsCommonManager_t* common_manager;
};

/**
 * @brief 创建WiFi NVS管理器实例
 */
WifiNvsManager_t* wifi_nvs_manager_create(void) {
    WifiNvsManager_t* wrapper = malloc(sizeof(WifiNvsManager_t));
    if (!wrapper) {
        return NULL;
    }
    
    wrapper->common_manager = nvs_common_manager_create(WIFI_CONFIG_NAMESPACE);
    if (!wrapper->common_manager) {
        free(wrapper);
        return NULL;
    }
    
    return wrapper;
}

/**
 * @brief 销毁WiFi NVS管理器实例
 */
void wifi_nvs_manager_destroy(WifiNvsManager_t* manager) {
    if (!manager) {
        return;
    }
    
    if (manager->common_manager) {
        nvs_common_manager_destroy(manager->common_manager);
    }
    
    free(manager);
}

/**
 * @brief 初始化WiFi NVS管理器
 */
esp_err_t wifi_nvs_manager_init(WifiNvsManager_t* manager) {
    if (!manager || !manager->common_manager) {
        return ESP_FAIL;
    }
    
    // 初始化通用管理器
    esp_err_t err = nvs_common_manager_init(manager->common_manager);
    if (err != ESP_OK) {
        return err;
    }
    
    // 打开NVS命名空间以便后续操作
    NvsCommonManager_t* wrapper = manager->common_manager;
    return nvs_base_open(wrapper->base_manager, false); // false表示可读写模式
}

/**
 * @brief 保存WiFi配置到NVS
 */
esp_err_t wifi_nvs_manager_save_config(WifiNvsManager_t* manager, 
                                      const char* ssid, 
                                      const char* password) {
    if (!manager || !manager->common_manager || !ssid || !password) {
        ESP_LOGE("WIFI_NVS", "Invalid parameters for save_config");
        return ESP_FAIL;
    }
    
    // 确保NVS命名空间已打开
    NvsCommonManager_t* wrapper = manager->common_manager;
    NvsBaseManager_t* base_manager = wrapper->base_manager;
    
    if (!base_manager || !base_manager->initialized) {
        ESP_LOGE("WIFI_NVS", "NVS not initialized");
        return ESP_FAIL;
    }
    
    // 强制关闭并重新打开NVS命名空间（使用全新的句柄）
    ESP_LOGI("WIFI_NVS", "Forcing reopen of NVS namespace to ensure clean state");
    if (base_manager->opened) {
        nvs_base_close(base_manager);
    }
    
    esp_err_t open_err = nvs_base_open(base_manager, false);
    if (open_err != ESP_OK) {
        ESP_LOGE("WIFI_NVS", "Failed to open NVS namespace: %s", esp_err_to_name(open_err));
        return open_err;
    }
    
    // 先保存SSID
    ESP_LOGI("WIFI_NVS", "Saving SSID: %s", ssid);
    esp_err_t err = nvs_common_manager_save_str(manager->common_manager, 
                                               WIFI_CONFIG_SSID_KEY, 
                                               ssid);
    if (err != ESP_OK) {
        ESP_LOGE("WIFI_NVS", "Failed to save SSID: %s", esp_err_to_name(err));
        return err;
    }
    
    // 再保存密码
    err = nvs_common_manager_save_str(manager->common_manager, 
                                      WIFI_CONFIG_PASS_KEY, 
                                      password);
    if (err != ESP_OK) {
        ESP_LOGE("WIFI_NVS", "Failed to save password: %s", esp_err_to_name(err));
        return err;
    }
    
    // 显式提交更改到NVS闪存
    esp_err_t commit_err = nvs_base_commit(base_manager);
    if (commit_err != ESP_OK) {
        ESP_LOGE("WIFI_NVS", "Failed to commit NVS changes: %s", esp_err_to_name(commit_err));
        return commit_err;
    }
    
    // 关闭NVS句柄以确保更改被完全写入
    nvs_base_close(base_manager);
    
    // 重新打开NVS以只读模式验证保存结果
    commit_err = nvs_base_open(base_manager, true);
    if (commit_err != ESP_OK) {
        ESP_LOGE("WIFI_NVS", "Failed to reopen NVS for verification: %s", esp_err_to_name(commit_err));
    } else {
        // 使用实际的加载操作来验证，而不仅仅是检查存在性
        char loaded_ssid[32] = {0};
        char loaded_password[64] = {0};
        
        err = nvs_common_manager_load_str(manager->common_manager, WIFI_CONFIG_SSID_KEY, loaded_ssid, sizeof(loaded_ssid));
        esp_err_t pass_err = nvs_common_manager_load_str(manager->common_manager, WIFI_CONFIG_PASS_KEY, loaded_password, sizeof(loaded_password));
        
        if (err == ESP_OK && pass_err == ESP_OK) {
            // 验证成功 - 移除了详细日志
        } else {
            ESP_LOGW("WIFI_NVS", "Verification failed - SSID load: %s, Password load: %s", 
                     esp_err_to_name(err), esp_err_to_name(pass_err));
        }
        
        // 关闭并重新打开为读写模式
        nvs_base_close(base_manager);
        nvs_base_open(base_manager, false);
    }
    
    // 再次强制提交以确保所有操作完成
    commit_err = nvs_base_commit(base_manager);
    if (commit_err != ESP_OK) {
        ESP_LOGE("WIFI_NVS", "Final commit failed: %s", esp_err_to_name(commit_err));
    }
    return ESP_OK;
}

/**
 * @brief 从NVS加载WiFi配置
 */
esp_err_t wifi_nvs_manager_load_config(WifiNvsManager_t* manager, 
                                      char* ssid, 
                                      size_t ssid_len, 
                                      char* password, 
                                      size_t password_len) {
    if (!manager || !manager->common_manager || !ssid || !password) {
        return ESP_FAIL;
    }
    
    // 确保NVS命名空间已打开
    NvsBaseManager_t* base_manager = manager->common_manager->base_manager;
    if (!base_manager || !base_manager->initialized) {
        ESP_LOGE("WIFI_NVS", "NVS not initialized");
        return ESP_FAIL;
    }
    
    if (!base_manager->opened) {
        esp_err_t open_err = nvs_base_open(base_manager, true); // 只读模式
        if (open_err != ESP_OK) {
            ESP_LOGE("WIFI_NVS", "Failed to reopen NVS namespace: %s", esp_err_to_name(open_err));
            return open_err;
        }
    }
    
    // 先加载SSID
    esp_err_t err = nvs_common_manager_load_str(manager->common_manager, 
                                               WIFI_CONFIG_SSID_KEY, 
                                               ssid, 
                                               ssid_len);
    if (err != ESP_OK) {
        ESP_LOGE("WIFI_NVS", "Failed to load SSID: %s", esp_err_to_name(err));
        return err;
    }
    
    // 再加载密码
    err = nvs_common_manager_load_str(manager->common_manager, 
                                      WIFI_CONFIG_PASS_KEY, 
                                      password, 
                                      password_len);
    if (err != ESP_OK) {
        ESP_LOGE("WIFI_NVS", "Failed to load password: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief 检查WiFi配置是否存在
 */
bool wifi_nvs_manager_has_config(WifiNvsManager_t* manager) {
    if (!manager || !manager->common_manager) {
        return false;
    }
    
    // 确保NVS命名空间已打开
    NvsBaseManager_t* base_manager = manager->common_manager->base_manager;
    if (!base_manager || !base_manager->initialized) {
        ESP_LOGE("WIFI_NVS", "NVS not initialized");
        return false;
    }
    
    if (!base_manager->opened) {
        esp_err_t open_err = nvs_base_open(base_manager, true); // 只读模式
        if (open_err != ESP_OK) {
            ESP_LOGE("WIFI_NVS", "Failed to reopen NVS namespace: %s", esp_err_to_name(open_err));
            return false;
        }
    }
    
    // 检查SSID和密码是否都存在
    return (nvs_common_manager_exists(manager->common_manager, WIFI_CONFIG_SSID_KEY) && 
            nvs_common_manager_exists(manager->common_manager, WIFI_CONFIG_PASS_KEY));
}

/**
 * @brief 清除WiFi配置
 */
esp_err_t wifi_nvs_manager_clear_config(WifiNvsManager_t* manager) {
    if (!manager || !manager->common_manager) {
        return ESP_FAIL;
    }
    
    // 先删除SSID
    esp_err_t err = nvs_common_manager_erase(manager->common_manager, WIFI_CONFIG_SSID_KEY);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        return err;
    }
    
    // 再删除密码
    err = nvs_common_manager_erase(manager->common_manager, WIFI_CONFIG_PASS_KEY);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        return err;
    }
    
    return ESP_OK;
}