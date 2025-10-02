/*
 * 初始化管理器模块实现
 * 用于协调和管理不同硬件模块的初始化顺序和依赖关系
 */
#include "init_manager.h"
#include "esp_log.h"
#include "string.h"

// 日志标签
static const char *TAG = "INIT_MANAGER";

// 模块描述符数组
static module_init_desc_t g_module_descriptors[MODULE_MAX];
static int g_registered_module_count = 0;
static SemaphoreHandle_t g_manager_mutex = NULL;

/**
 * @brief 检查模块依赖是否满足
 * @param desc 模块描述符
 * @return true 依赖满足，false 依赖不满足
 */
static bool check_dependencies(const module_init_desc_t *desc) {
    for (int i = 0; i < desc->dependency_count; i++) {
        module_id_t dep_id = desc->dependencies[i];
        if (g_module_descriptors[dep_id].state != INIT_STATE_COMPLETED) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 模块初始化任务
 * @param arg 模块描述符指针
 */
static void module_init_task(void *arg) {
    module_init_desc_t *desc = (module_init_desc_t *)arg;
    const char *module_names[] = {
        "NVS", "WS2812", "OLED", "KEYBOARD", "WIFI", "SLEEP_MODE"
    };

    // 等待依赖模块初始化完成
    while (!check_dependencies(desc)) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    // 执行模块初始化
    desc->state = INIT_STATE_IN_PROGRESS;

    esp_err_t err = desc->init_func();
    if (err == ESP_OK) {
        desc->state = INIT_STATE_COMPLETED;
        // 释放就绪信号量
        xSemaphoreGive(desc->ready_sem);
    } else {
        desc->state = INIT_STATE_FAILED;
        ESP_LOGE(TAG, "Failed to initialize %s module: %s", 
                 module_names[desc->module_id], esp_err_to_name(err));
    }

    vTaskDelete(NULL);
}

/**
 * @brief 初始化管理器
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_init(void) {
    // 初始化模块描述符数组
    memset(g_module_descriptors, 0, sizeof(g_module_descriptors));
    g_registered_module_count = 0;

    // 创建互斥锁
    g_manager_mutex = xSemaphoreCreateMutex();
    if (!g_manager_mutex) {
        ESP_LOGE(TAG, "Failed to create manager mutex");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief 注册模块初始化描述符
 * @param desc 模块初始化描述符
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_register_module(const module_init_desc_t *desc) {
    if (!desc || desc->module_id >= MODULE_MAX) {
        ESP_LOGE(TAG, "Invalid module descriptor");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_manager_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    // 检查模块是否已经注册
    if (g_module_descriptors[desc->module_id].init_func != NULL) {
        ESP_LOGE(TAG, "Module %d already registered", desc->module_id);
        xSemaphoreGive(g_manager_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    // 复制模块描述符
    memcpy(&g_module_descriptors[desc->module_id], desc, sizeof(module_init_desc_t));

    // 创建就绪信号量
    g_module_descriptors[desc->module_id].ready_sem = xSemaphoreCreateBinary();
    if (!g_module_descriptors[desc->module_id].ready_sem) {
        ESP_LOGE(TAG, "Failed to create semaphore for module %d", desc->module_id);
        xSemaphoreGive(g_manager_mutex);
        return ESP_FAIL;
    }

    g_registered_module_count++;
    xSemaphoreGive(g_manager_mutex);
    return ESP_OK;
}

/**
 * @brief 启动所有模块的初始化过程
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_start_init(void) {
    if (g_registered_module_count == 0) {
        ESP_LOGE(TAG, "No modules registered");
        return ESP_ERR_INVALID_STATE;
    }

    // 创建所有模块的初始化任务
    for (int i = 0; i < MODULE_MAX; i++) {
        if (g_module_descriptors[i].init_func != NULL && 
            g_module_descriptors[i].state == INIT_STATE_UNINITIALIZED) {
            // 创建初始化任务，但不立即执行（任务内部会等待依赖）
            if (xTaskCreate(module_init_task, "module_init_task", 4096, 
                           &g_module_descriptors[i], 5, NULL) != pdPASS) {
                ESP_LOGE(TAG, "Failed to create init task for module %d", i);
                return ESP_FAIL;
            }
        }
    }

    ESP_LOGI(TAG, "All module initialization tasks created");
    return ESP_OK;
}

/**
 * @brief 等待指定模块初始化完成
 * @param module_id 模块ID
 * @param timeout_ms 超时时间(毫秒)
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_wait_for_module(module_id_t module_id, int timeout_ms) {
    if (module_id >= MODULE_MAX || g_module_descriptors[module_id].ready_sem == NULL) {
        ESP_LOGE(TAG, "Invalid module ID or module not registered");
        return ESP_ERR_INVALID_ARG;
    }

    // 检查模块是否已经初始化完成
    if (g_module_descriptors[module_id].state == INIT_STATE_COMPLETED) {
        return ESP_OK;
    }

    // 等待模块初始化完成信号量
    TickType_t timeout_ticks = timeout_ms / portTICK_PERIOD_MS;
    if (xSemaphoreTake(g_module_descriptors[module_id].ready_sem, timeout_ticks) != pdTRUE) {
        ESP_LOGE(TAG, "Timeout waiting for module %d initialization", module_id);
        return ESP_ERR_TIMEOUT;
    }

    // 再次检查状态，确保初始化成功
    if (g_module_descriptors[module_id].state != INIT_STATE_COMPLETED) {
        ESP_LOGE(TAG, "Module %d initialization failed", module_id);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief 获取指定模块的初始化状态
 * @param module_id 模块ID
 * @param state 输出参数，用于存储模块状态
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_get_module_state(module_id_t module_id, init_state_t *state) {
    if (module_id >= MODULE_MAX || !state) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_manager_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    *state = g_module_descriptors[module_id].state;

    xSemaphoreGive(g_manager_mutex);
    return ESP_OK;
}

/**
 * @brief 应用指定模块的配置
 * @param module_id 模块ID
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_apply_module_config(module_id_t module_id) {
    if (module_id >= MODULE_MAX || g_module_descriptors[module_id].apply_config_func == NULL) {
        ESP_LOGE(TAG, "Invalid module ID or no config function");
        return ESP_ERR_INVALID_ARG;
    }

    // 等待模块初始化完成
    if (init_manager_wait_for_module(module_id, 5000) != ESP_OK) {
        ESP_LOGE(TAG, "Module %d not initialized, cannot apply config", module_id);
        return ESP_FAIL;
    }

    // 应用配置
    esp_err_t err = g_module_descriptors[module_id].apply_config_func();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply configuration for module %d: %s", 
                 module_id, esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/**
 * @brief 应用所有已初始化模块的配置
 * @return ESP_OK 成功，其他失败
 */
esp_err_t init_manager_apply_all_configs(void) {
    esp_err_t overall_err = ESP_OK;

    // 应用所有模块的配置
    for (int i = 0; i < MODULE_MAX; i++) {
        if (g_module_descriptors[i].apply_config_func != NULL && 
            g_module_descriptors[i].state == INIT_STATE_COMPLETED) {
            esp_err_t err = g_module_descriptors[i].apply_config_func();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to apply configuration for module %d: %s", 
                         i, esp_err_to_name(err));
                overall_err = err;
            } else {
                ESP_LOGI(TAG, "Configuration applied successfully for module %d", i);
            }
        }
    }

    return overall_err;
}