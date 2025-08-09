#ifndef _KEYMAP_NVS_MANAGER_
#define _KEYMAP_NVS_MANAGER_

#include "nvs_manager_base.h"

/**
 * @brief 按键映射NVS管理器
 * 
 * 这个类继承自NvsManagerBase，专门用于管理按键映射数据的存储和读取。
 * 提供了针对按键映射的特定功能，如按层保存/加载、重置为默认值等。
 */
class KeymapNvsManager : public NvsManagerBase {
public:
    /**
     * @brief 构造函数
     * @param namespace_name NVS命名空间名称，默认为"keymaps"
     * @param key_prefix 键名前缀，默认为"keymap_"
     * @param num_keys 按键数量
     * @param num_layers 层数
     */
    KeymapNvsManager(const char* namespace_name = "keymaps", 
                     const char* key_prefix = "keymap_", 
                     uint8_t num_keys = 17, 
                     uint8_t num_layers = 2);
    
    /**
     * @brief 析构函数
     */
    virtual ~KeymapNvsManager();
    
    /**
     * @brief 初始化按键映射管理器
     * @param default_keymaps 默认按键映射数组
     * @param keymaps 运行时按键映射数组
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t init_with_keymaps(const uint16_t* default_keymaps, uint16_t* keymaps);
    
    // 使用基类的init方法，避免虚函数隐藏警告
    using NvsManagerBase::init;
    
    /**
     * @brief 保存指定层的按键映射
     * @param layer 层索引
     * @param keymap 按键映射数组
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t save_keymap(uint8_t layer, const uint16_t* keymap);
    
    /**
     * @brief 加载指定层的按键映射
     * @param layer 层索引
     * @param keymap 用于存储按键映射的数组
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t load_keymap(uint8_t layer, uint16_t* keymap);
    
    /**
     * @brief 重置指定层的按键映射为默认值
     * @param layer 层索引
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t reset_keymap_to_default(uint8_t layer);
    
    /**
     * @brief 加载所有层的按键映射
     * @param keymaps 运行时按键映射数组
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t load_all_keymaps(uint16_t* keymaps);
    
    /**
     * @brief 保存所有层的按键映射
     * @param keymaps 按键映射数组
     * @return ESP_OK 成功，其他失败
     */
    virtual esp_err_t save_all_keymaps(const uint16_t* keymaps);
    
    /**
     * @brief 检查指定层的按键映射是否存在
     * @param layer 层索引
     * @return true 存在，false 不存在
     */
    virtual bool keymap_exists(uint8_t layer);
    
    /**
     * @brief 获取按键数量
     * @return 按键数量
     */
    virtual uint8_t get_num_keys() const { return num_keys; }
    
    /**
     * @brief 获取层数
     * @return 层数
     */
    virtual uint8_t get_num_layers() const { return num_layers; }
    
    /**
     * @brief 测试按键映射配置功能
     * @param keymaps 运行时按键映射数组
     */
    virtual void test_keymap_config(uint16_t* keymaps);

private:
    const char* key_prefix;     // 键名前缀
    uint8_t num_keys;            // 按键数量
    uint8_t num_layers;          // 层数
    const uint16_t* default_keymaps; // 默认按键映射数组
    
    /**
     * @brief 生成层对应的键名
     * @param layer 层索引
     * @param key_buffer 键名缓冲区
     * @param buffer_size 缓冲区大小
     */
    virtual void generate_key_name(uint8_t layer, char* key_buffer, size_t buffer_size);
    
    /**
     * @brief 检查层索引是否有效
     * @param layer 层索引
     * @return true 有效，false 无效
     */
    virtual bool is_valid_layer(uint8_t layer) const;
};

#endif // _KEYMAP_NVS_MANAGER_