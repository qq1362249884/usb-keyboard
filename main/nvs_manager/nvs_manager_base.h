#ifndef _NVS_MANAGER_BASE_
#define _NVS_MANAGER_BASE_

#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

/**
 * @brief NVS管理器抽象基类
 * 
 * 这个类提供了NVS操作的基本接口，包括初始化、读写、删除等通用功能。
 * 衍生类可以实现特定类型的数据管理。
 */
class NvsManagerBase {
public:
    /**
     * @brief 构造函数
     * @param namespace_name NVS命名空间名称
     */
    NvsManagerBase(const char* namespace_name);
    
    /**
     * @brief 虚析构函数
     */
    virtual ~NvsManagerBase();
    
    /**
     * @brief 初始化NVS
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t init();
    
    /**
     * @brief 保存数据到NVS
     * @param key 键名
     * @param data 数据指针
     * @param size 数据大小
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t save(const char* key, const void* data, size_t size);
    
    /**
     * @brief 从NVS加载数据
     * @param key 键名
     * @param data 数据缓冲区
     * @param size 数据大小
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t load(const char* key, void* data, size_t size);
    
    /**
     * @brief 从NVS删除数据
     * @param key 键名
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t erase(const char* key);
    
    /**
     * @brief 检查键是否存在
     * @param key 键名
     * @return true 存在，false 不存在
     */
    virtual bool exists(const char* key);
    
    /**
     * @brief 提交所有更改
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t commit();
    
    /**
     * @brief 获取NVS错误信息的字符串描述
     * @param err 错误码
     * @return 错误信息字符串
     */
    virtual const char* get_error_string(esp_err_t err);
    
protected:
    const char* namespace_name;  // NVS命名空间名称
    nvs_handle_t nvs_handle;     // NVS句柄
    bool is_initialized;         // 是否已初始化
    
    /**
     * @brief 打开NVS
     * @param read_only 是否只读
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t open_nvs(bool read_only = false);
    
    /**
     * @brief 关闭NVS
     */
    virtual void close_nvs();
    
    /**
     * @brief 初始化NVS闪存
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t init_nvs_flash();
};

#endif // _NVS_MANAGER_BASE_