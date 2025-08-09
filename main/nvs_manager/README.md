# NVS Manager Module

## 概述

NVS Manager是一个ESP-IDF的NVS(Non-Volatile Storage)抽象层模块，提供了面向对象的NVS操作接口。该模块包含一个抽象基类`NvsManagerBase`和一个专门用于按键映射管理的衍生类`KeymapNvsManager`。

## 文件结构

```
nvs_manager/
├── nvs_manager_base.h      # NVS管理器抽象基类头文件
├── nvs_manager_base.cpp    # NVS管理器抽象基类实现
├── keymap_nvs_manager.h    # 按键映射NVS管理器头文件
├── keymap_nvs_manager.cpp  # 按键映射NVS管理器实现
├── example_usage.cpp       # 使用示例和兼容接口
├── CMakeLists.txt          # CMake配置文件
└── README.md               # 说明文档
```

## 特性

### NvsManagerBase (抽象基类)
- 提供通用的NVS操作接口
- 支持初始化、保存、加载、删除等基本操作
- 自动处理NVS闪存初始化和错误处理
- 支持自定义命名空间

### KeymapNvsManager (按键映射管理器)
- 继承自NvsManagerBase，专门用于按键映射管理
- 支持多层按键映射的存储和读取
- 提供默认值回退机制
- 支持批量操作所有层的按键映射
- 提供测试和调试功能

## 使用方法

### 1. 集成到项目

在主CMakeLists.txt中添加nvs_manager组件：

```cmake
# 在main/CMakeLists.txt中添加
idf_component_register(SRCS "main.c" ...)

# 或者将nvs_manager作为子目录添加
add_subdirectory(nvs_manager)
```

### 2. 基本使用

#### 使用KeymapNvsManager

```cpp
#include "keymap_nvs_manager.h"

// 定义默认按键映射
static const uint16_t default_keymaps[][NUM_KEYS] = {
    [0] = { KC_ESC, KC_A, KC_B, ... },
    [1] = { KC_ESC, KC_X, KC_Y, ... }
};

// 定义运行时按键映射
uint16_t keymaps[2][NUM_KEYS] = {0};

// 创建并初始化NVS管理器
KeymapNvsManager keymap_manager("keymaps", "keymap_", NUM_KEYS, 2);
esp_err_t err = keymap_manager.init(&default_keymaps[0][0], &keymaps[0][0]);

if (err == ESP_OK) {
    ESP_LOGI("MAIN", "NVS manager initialized successfully");
}
```

#### 保存和加载按键映射

```cpp
// 保存特定层的按键映射
err = keymap_manager.save_keymap(1, &keymaps[1][0]);

// 加载特定层的按键映射
err = keymap_manager.load_keymap(1, &keymaps[1][0]);

// 重置为默认值
err = keymap_manager.reset_keymap_to_default(1);

// 批量操作
err = keymap_manager.save_all_keymaps(&keymaps[0][0]);
err = keymap_manager.load_all_keymaps(&keymaps[0][0]);
```

### 3. 使用兼容接口

如果需要保持与原有代码的兼容性，可以使用example_usage.cpp中提供的兼容接口：

```cpp
#include "example_usage.cpp"

// 初始化（兼容原接口）
esp_err_t err = nvs_init();

// 保存按键映射（兼容原接口）
err = save_keymap_to_nvs(1, &keymaps[1][0]);

// 加载按键映射（兼容原接口）
err = load_keymap_from_nvs(1, &keymaps[1][0]);

// 测试配置（兼容原接口）
test_keymap_config();
```

### 4. 高级功能

```cpp
// 检查按键映射是否存在
bool exists = keymap_manager.keymap_exists(1);

// 获取配置信息
uint8_t num_keys = keymap_manager.get_num_keys();
uint8_t num_layers = keymap_manager.get_num_layers();

// 测试功能
keymap_manager.test_keymap_config(&keymaps[0][0]);
```

## API参考

### NvsManagerBase

#### 构造函数
```cpp
NvsManagerBase(const char* namespace_name);
```

#### 主要方法
- `init()`: 初始化NVS
- `save(key, data, size)`: 保存数据
- `load(key, data, size)`: 加载数据
- `erase(key)`: 删除数据
- `exists(key)`: 检查数据是否存在
- `commit()`: 提交更改

### KeymapNvsManager

#### 构造函数
```cpp
KeymapNvsManager(const char* namespace_name = "keymaps", 
                 const char* key_prefix = "keymap_", 
                 uint8_t num_keys = 17, 
                 uint8_t num_layers = 2);
```

#### 主要方法
- `init(default_keymaps, keymaps)`: 初始化并加载所有按键映射
- `save_keymap(layer, keymap)`: 保存特定层的按键映射
- `load_keymap(layer, keymap)`: 加载特定层的按键映射
- `reset_keymap_to_default(layer)`: 重置为默认值
- `save_all_keymaps(keymaps)`: 保存所有层的按键映射
- `load_all_keymaps(keymaps)`: 加载所有层的按键映射
- `keymap_exists(layer)`: 检查特定层按键映射是否存在
- `test_keymap_config(keymaps)`: 测试按键映射配置

## 迁移指南

### 从原有NVS代码迁移

1. **包含头文件**：
   ```cpp
   // 原有代码
   #include "nvs_flash.h"
   #include "nvs.h"
   
   // 新代码
   #include "keymap_nvs_manager.h"
   ```

2. **替换初始化**：
   ```cpp
   // 原有代码
   esp_err_t nvs_init(void) {
       esp_err_t err = nvs_flash_init();
       // ... 复杂的初始化逻辑
   }
   
   // 新代码
   KeymapNvsManager keymap_manager;
   esp_err_t err = keymap_manager.init(default_keymaps, keymaps);
   ```

3. **替换读写操作**：
   ```cpp
   // 原有代码
   esp_err_t save_keymap_to_nvs(uint8_t layer, const uint16_t* keymap) {
       // 复杂的NVS操作
   }
   
   // 新代码
   esp_err_t err = keymap_manager.save_keymap(layer, keymap);
   ```

4. **使用兼容接口**（如果需要保持兼容性）：
   ```cpp
   #include "example_usage.cpp"
   // 直接使用原有函数名，内部调用新的NVS管理器
   ```

## 错误处理

所有NVS操作都返回`esp_err_t`类型，可以使用`get_error_string()`方法获取错误描述：

```cpp
esp_err_t err = keymap_manager.save_keymap(1, keymap);
if (err != ESP_OK) {
    ESP_LOGE("MAIN", "Failed to save keymap: %s", 
             keymap_manager.get_error_string(err));
}
```

## 调试和日志

模块提供详细的调试日志，可通过ESP-IDF的日志系统查看：

```bash
# 查看NVS相关日志
idf.py monitor | grep "NVS_BASE\|KEYMAP_NVS"
```

## 注意事项

1. **内存管理**：NVS管理器使用动态内存分配，记得在适当的时候删除实例
2. **线程安全**：当前实现不是线程安全的，多线程环境下需要添加互斥锁
3. **NVS空间**：确保NVS分区有足够的空间存储按键映射数据
4. **初始化顺序**：确保在使用NVS管理器之前正确初始化NVS闪存

## 扩展开发

### 创建自定义NVS管理器

可以继承`NvsManagerBase`来创建针对特定数据类型的NVS管理器：

```cpp
class CustomNvsManager : public NvsManagerBase {
public:
    CustomNvsManager(const char* namespace_name) 
        : NvsManagerBase(namespace_name) {}
    
    esp_err_t save_custom_data(const CustomData* data) {
        return save("custom_data", data, sizeof(CustomData));
    }
    
    esp_err_t load_custom_data(CustomData* data) {
        return load("custom_data", data, sizeof(CustomData));
    }
};
```

## 许可证

本模块遵循与主项目相同的许可证。