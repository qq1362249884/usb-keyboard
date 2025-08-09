# 在C文件中调用C++ NVS管理器

本文档说明如何在纯C代码中调用C++实现的NVS管理器功能。

## 概述

由于ESP-IDF项目通常使用C语言，但我们的NVS管理器使用C++实现，我们需要一个C包装器来桥接两种语言。C包装器提供了纯C接口，隐藏了C++的实现细节。

## 文件结构

```
nvs_manager/
├── nvs_manager_base.h/cpp    # C++基础类
├── keymap_nvs_manager.h/cpp  # C++按键映射管理器
├── c_wrapper.h/cpp           # C包装器
├── c_example_usage.c         # C语言使用示例
└── CMakeLists.txt            # 编译配置
```

## 使用方法

### 1. 包含头文件

在你的C文件中包含C包装器头文件：

```c
#include "c_wrapper.h"
```

### 2. 创建NVS管理器

```c
KeymapNvsManager_t* manager = keymap_nvs_manager_create(
    "keymaps",           // NVS命名空间
    "keymap_",           // 键前缀
    NUM_KEYS,            // 按键数量
    NUM_LAYERS           // 层数
);
```

### 3. 初始化管理器

```c
esp_err_t err = keymap_nvs_manager_init(
    manager, 
    &default_keymaps[0][0],  // 默认按键映射
    &keymaps[0][0]           // 运行时按键映射
);
```

### 4. 使用NVS功能

```c
// 保存按键映射
err = keymap_nvs_manager_save(manager, layer, &keymaps[layer][0]);

// 加载按键映射
err = keymap_nvs_manager_load(manager, layer, &keymaps[layer][0]);

// 重置为默认值
err = keymap_nvs_manager_reset(manager, layer);

// 检查是否存在
int exists = keymap_nvs_manager_exists(manager, layer);

// 运行测试
keymap_nvs_manager_test_config(manager, &keymaps[0][0]);
```

### 5. 清理资源

```c
keymap_nvs_manager_destroy(manager);
```

## 完整示例

参考 `c_example_usage.c` 文件，它包含了：
- 完整的初始化和清理流程
- 所有NVS操作的C接口
- 兼容原有代码的包装函数
- 测试函数

## 兼容原有接口

为了保持与原有代码的兼容性，`c_example_usage.c` 提供了以下兼容函数：

```c
// 兼容原有的nvs_init
esp_err_t nvs_init_c(void);

// 兼容原有的save_keymap_to_nvs
esp_err_t save_keymap_to_nvs_c(uint8_t layer, const uint16_t* keymap);

// 兼容原有的load_keymap_from_nvs
esp_err_t load_keymap_from_nvs_c(uint8_t layer, uint16_t* keymap);

// 兼容原有的reset_keymap_to_default
esp_err_t reset_keymap_to_default_c(uint8_t layer);

// 兼容原有的test_keymap_config
void test_keymap_config_c(void);
```

## 编译配置

确保 `CMakeLists.txt` 包含所有必要的文件：

```cmake
# 源文件
set(NVS_MANAGER_SRCS
    nvs_manager_base.cpp
    keymap_nvs_manager.cpp
    c_wrapper.cpp
    c_example_usage.c
)

idf_component_register(
    SRCS ${NVS_MANAGER_SRCS}
    INCLUDE_DIRS "."
    REQUIRES nvs_flash
)
```

## 错误处理

所有C接口函数都返回 `esp_err_t` 类型，可以使用 `keymap_nvs_manager_get_error_string()` 获取错误描述：

```c
esp_err_t err = keymap_nvs_manager_save(manager, layer, keymap);
if (err != ESP_OK) {
    const char* error_str = keymap_nvs_manager_get_error_string(manager, err);
    ESP_LOGE(TAG, "Save failed: %s", error_str);
}
```

## 注意事项

1. **内存管理**：确保在程序结束时调用 `keymap_nvs_manager_destroy()` 释放资源
2. **线程安全**：C包装器内部没有实现线程同步，如有需要请在调用层添加同步机制
3. **错误检查**：所有函数都应检查返回值
4. **句柄有效性**：使用句柄前检查是否为NULL

## 迁移指南

### 从原有C代码迁移

1. 将原有的NVS相关函数调用替换为C包装器接口
2. 添加 `#include "c_wrapper.h"`
3. 在程序初始化时创建NVS管理器
4. 在程序结束时销毁NVS管理器
5. 使用兼容函数保持现有代码不变

### 示例迁移

**原有代码：**
```c
void save_my_keymap(uint8_t layer, uint16_t* keymap) {
    save_keymap_to_nvs(layer, keymap);
}
```

**迁移后代码：**
```c
void save_my_keymap(uint8_t layer, uint16_t* keymap) {
    save_keymap_to_nvs_c(layer, keymap);
}
```

## 总结

通过C包装器，我们成功地在C代码中使用了C++实现的NVS管理器。这种方法提供了以下优势：

1. **语言兼容**：C代码可以调用C++实现
2. **接口简化**：C接口更简单易用
3. **向后兼容**：保持与原有代码的兼容性
4. **类型安全**：使用不透明句柄提高类型安全性
5. **错误处理**：统一的错误处理机制

这种模式可以应用于其他需要在C代码中使用C++功能的场景。