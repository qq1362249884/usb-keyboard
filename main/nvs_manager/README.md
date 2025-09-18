# NVS Manager 模块

## 概述

NVS Manager是一个ESP-IDF的NVS(Non-Volatile Storage)抽象层模块，提供了纯C语言的NVS操作接口。该模块包含一个基础NVS管理器接口`nvs_manager`，以及两个特定功能的管理器：`keymap_nvs_manager`（按键映射管理）和`wifi_nvs_manager`（WiFi配置管理）。所有组件均采用纯C实现，简化了项目依赖并提高了代码的一致性。

## 文件结构

```
nvs_manager/
├── nvs_manager.h          # 基础NVS管理器接口
├── nvs_manager.c          # 基础NVS管理器实现
├── keymap_nvs_manager.h   # 按键映射NVS管理器头文件
├── keymap_nvs_manager.c   # 按键映射NVS管理器纯C实现
├── wifi_nvs_manager.h     # WiFi配置NVS管理器头文件
├── wifi_nvs_manager.c     # WiFi配置NVS管理器纯C实现
├── CMakeLists.txt         # CMake配置文件
└── README.md              # 说明文档
```

> 注意：之前的`nvs_base.c`、`nvs_base.h`、`nvs_common.c`和`nvs_common.h`四个文件已合并为`nvs_manager.c`和`nvs_manager.h`两个文件。所有原来的API函数和功能都保持不变，使用方式与之前完全一致。

## 特性

### nvs_manager (基础NVS接口)
- 提供底层NVS操作的通用C接口
- 支持二进制数据、字符串、整数等多种数据类型
- 自动处理NVS闪存初始化和错误处理
- 支持自定义命名空间
- 简化的内存管理接口

### keymap_nvs_manager (按键映射管理器)
- 基于基础NVS接口实现，专门用于按键映射管理
- 纯C实现，轻量高效
- 支持多层按键映射的存储和读取
- 提供默认值回退机制
- 支持单一层或所有层的批量操作
- 提供按键数量和层数的查询接口
- 内置配置测试功能

### wifi_nvs_manager (WiFi配置管理器)
- 基于基础NVS接口实现，专门用于WiFi配置管理
- 纯C实现，独立于C++运行时
- 提供SSID和密码的保存/加载接口
- 支持配置存在性检查
- 支持一键清除配置
- 极简API设计，易于集成

### 整体优势
- 纯C实现，无C++依赖，减小固件体积
- 接口统一，使用模式一致
- 错误处理完善，返回ESP标准错误码
- 代码结构清晰，易于扩展新功能
- 内存占用低，适合嵌入式环境

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

#### 使用按键映射管理器

```c
#include "keymap_nvs_manager.h"

// 定义默认按键映射
#define NUM_KEYS 17
#define NUM_LAYERS 2

static const uint16_t default_keymaps[][NUM_KEYS] = {
    { KC_ESC, KC_A, KC_B, ... },
    { KC_ESC, KC_X, KC_Y, ... }
};

// 定义运行时按键映射
uint16_t keymaps[NUM_LAYERS][NUM_KEYS] = {0};

// 创建NVS管理器实例
KeymapNvsManager_t* keymap_manager = keymap_nvs_manager_create("keymaps", "keymap_", NUM_KEYS, NUM_LAYERS);

// 初始化管理器
esp_err_t err = keymap_nvs_manager_init(keymap_manager, &default_keymaps[0][0], &keymaps[0][0]);

if (err == ESP_OK) {
    ESP_LOGI("MAIN", "按键映射管理器初始化成功");
}
```

#### 保存和加载按键映射

```c
// 保存特定层的按键映射
err = keymap_nvs_manager_save_keymap(keymap_manager, 1, &keymaps[1][0]);

// 加载特定层的按键映射
err = keymap_nvs_manager_load_keymap(keymap_manager, 1, &keymaps[1][0]);

// 重置为默认值
err = keymap_nvs_manager_reset_keymap_to_default(keymap_manager, 1);

// 批量操作
err = keymap_nvs_manager_save_all_keymaps(keymap_manager, &keymaps[0][0]);
err = keymap_nvs_manager_load_all_keymaps(keymap_manager, &keymaps[0][0]);

// 销毁管理器实例（程序结束时）
keymap_nvs_manager_destroy(keymap_manager);
```

### 3. 使用WiFi NVS管理器

```c
#include "wifi_nvs_manager.h"

// 创建WiFi NVS管理器实例
WifiNvsManager_t* wifi_nvs_manager = wifi_nvs_manager_create();

// 初始化管理器
esp_err_t err = wifi_nvs_manager_init(wifi_nvs_manager);

if (err == ESP_OK) {
    ESP_LOGI("WIFI_APP", "WiFi NVS manager initialized successfully");
}

// 保存WiFi配置
char ssid[] = "MyWiFiNetwork";
char password[] = "MyPassword123";
err = wifi_nvs_manager_save_config(wifi_nvs_manager, ssid, password);

// 检查WiFi配置是否存在
bool has_config = wifi_nvs_manager_has_config(wifi_nvs_manager);

// 加载WiFi配置
char loaded_ssid[32] = {0};
char loaded_password[64] = {0};
err = wifi_nvs_manager_load_config(wifi_nvs_manager, loaded_ssid, loaded_password);

// 清除WiFi配置
err = wifi_nvs_manager_clear_config(wifi_nvs_manager);

// 销毁管理器实例（程序结束时）
wifi_nvs_manager_destroy(wifi_nvs_manager);
```

### 4. 高级功能

```c
// 检查按键映射是否存在
bool exists = keymap_nvs_manager_keymap_exists(keymap_manager, 1);

// 获取配置信息
uint8_t num_keys = keymap_nvs_manager_get_num_keys(keymap_manager);
uint8_t num_layers = keymap_nvs_manager_get_num_layers(keymap_manager);

// 测试功能
keymap_nvs_manager_test_keymap_config(keymap_manager, &keymaps[0][0]);
```

## API参考

### 按键映射管理器API

#### 创建和销毁
```c
// 创建按键映射管理器实例
KeymapNvsManager_t* keymap_nvs_manager_create(const char* namespace_name, 
                                             const char* key_prefix, 
                                             uint8_t num_keys, 
                                             uint8_t num_layers);

// 销毁按键映射管理器实例
void keymap_nvs_manager_destroy(KeymapNvsManager_t* manager);
```

#### 初始化
```c
// 初始化管理器并加载默认配置
esp_err_t keymap_nvs_manager_init(KeymapNvsManager_t* manager, 
                                 const uint16_t* default_keymaps, 
                                 uint16_t* keymaps);
```

#### 按键映射操作
```c
// 保存特定层的按键映射
esp_err_t keymap_nvs_manager_save_keymap(KeymapNvsManager_t* manager, 
                                        uint8_t layer, 
                                        const uint16_t* keymap);

// 加载特定层的按键映射
esp_err_t keymap_nvs_manager_load_keymap(KeymapNvsManager_t* manager, 
                                        uint8_t layer, 
                                        uint16_t* keymap);

// 将特定层重置为默认值
esp_err_t keymap_nvs_manager_reset_keymap_to_default(KeymapNvsManager_t* manager, 
                                                   uint8_t layer);

// 保存所有层的按键映射
esp_err_t keymap_nvs_manager_save_all_keymaps(KeymapNvsManager_t* manager, 
                                             const uint16_t* keymaps);

// 加载所有层的按键映射
esp_err_t keymap_nvs_manager_load_all_keymaps(KeymapNvsManager_t* manager, 
                                             uint16_t* keymaps);
```

#### 查询和测试
```c
// 检查特定层按键映射是否存在
bool keymap_nvs_manager_keymap_exists(KeymapNvsManager_t* manager, uint8_t layer);

// 获取按键数量
uint8_t keymap_nvs_manager_get_num_keys(KeymapNvsManager_t* manager);

// 获取层数
uint8_t keymap_nvs_manager_get_num_layers(KeymapNvsManager_t* manager);

// 测试按键映射配置
esp_err_t keymap_nvs_manager_test_keymap_config(KeymapNvsManager_t* manager, 
                                              const uint16_t* keymaps);
```

### WiFi管理器API

#### 创建和销毁
```c
// 创建WiFi NVS管理器实例
WifiNvsManager_t* wifi_nvs_manager_create(void);

// 销毁WiFi NVS管理器实例
void wifi_nvs_manager_destroy(WifiNvsManager_t* manager);
```

#### 初始化
```c
// 初始化WiFi NVS管理器
esp_err_t wifi_nvs_manager_init(WifiNvsManager_t* manager);
```

#### 配置操作
```c
// 保存WiFi配置
esp_err_t wifi_nvs_manager_save_config(WifiNvsManager_t* manager, 
                                     const char* ssid, 
                                     const char* password);

// 加载WiFi配置
esp_err_t wifi_nvs_manager_load_config(WifiNvsManager_t* manager, 
                                     char* ssid, 
                                     char* password);

// 检查WiFi配置是否存在
bool wifi_nvs_manager_has_config(WifiNvsManager_t* manager);

// 清除WiFi配置
esp_err_t wifi_nvs_manager_clear_config(WifiNvsManager_t* manager);
```

## 迁移指南

### 从原有NVS代码迁移

1. **包含头文件**：
   ```c
   // 原有代码
   #include "nvs_flash.h"
   #include "nvs.h"
   
   // 新代码
   #include "keymap_nvs_manager.h"
   ```

2. **替换初始化**：
   ```c
   // 原有代码
   esp_err_t nvs_init(void) {
       esp_err_t err = nvs_flash_init();
       // ... 复杂的初始化逻辑
   }
   
   // 新代码
   KeymapNvsManager_t* keymap_manager = keymap_nvs_manager_create("keymaps", "keymap_", NUM_KEYS, NUM_LAYERS);
   esp_err_t err = keymap_nvs_manager_init(keymap_manager, default_keymaps, keymaps);
   ```

3. **替换读写操作**：
   ```c
   // 原有代码
   esp_err_t save_keymap_to_nvs(uint8_t layer, const uint16_t* keymap) {
       // 复杂的NVS操作
   }
   
   // 新代码
   esp_err_t err = keymap_nvs_manager_save_keymap(keymap_manager, layer, keymap);
   ```

4. **清理资源**：
   ```c
   // 程序结束时销毁管理器实例
   keymap_nvs_manager_destroy(keymap_manager);
   ```

## 错误处理

所有NVS操作都返回`esp_err_t`类型，可直接使用ESP-IDF的错误处理机制：

```c
esp_err_t err = keymap_nvs_manager_save_keymap(keymap_manager, 1, keymap);
if (err != ESP_OK) {
    ESP_LOGE("MAIN", "保存按键映射失败: %s", esp_err_to_name(err));
}
```

## 调试和日志

模块提供详细的调试日志，可通过ESP-IDF的日志系统查看：

```bash
# 查看按键映射NVS相关日志
idf.py monitor | grep "KEYMAP_NVS_MANAGER"

# 查看WiFi NVS相关日志
idf.py monitor | grep "WIFI_NVS_MANAGER"

# 查看所有NVS相关日志
idf.py monitor | grep "NVS_"
```

## 注意事项

1. **内存管理**：所有NVS管理器实例使用动态内存分配，使用完毕后必须调用对应的destroy函数释放资源
2. **线程安全**：当前实现不是线程安全的，多线程环境下需要添加互斥锁保护共享资源
3. **NVS空间**：确保NVS分区有足够空间，建议预留至少2KB空间用于按键映射和WiFi配置存储
4. **初始化顺序**：必须先调用nvs_flash_init()初始化NVS闪存，再初始化各个NVS管理器
5. **错误处理**：所有API返回esp_err_t类型，使用前务必检查返回值

## 扩展开发

### 创建自定义NVS管理器

可以基于基础NVS接口创建针对特定数据类型的自定义NVS管理器：

```c
#include "nvs_manager.h"
#include <string.h>

// 定义自定义数据结构
typedef struct {
    int version;
    char name[32];
    uint32_t flags;
} CustomData;

// 定义管理器结构体
typedef struct {
    NvsCommonManager_t* nvs_common;
    const char* data_key;
} CustomNvsManager;

// 创建管理器实例
CustomNvsManager* custom_nvs_manager_create(const char* namespace_name, const char* data_key) {
    CustomNvsManager* manager = malloc(sizeof(CustomNvsManager));
    if (!manager) return NULL;

    manager->nvs_common = nvs_manager_create(namespace_name);
    manager->data_key = strdup(data_key);
    return manager;
}

// 初始化管理器
esp_err_t custom_nvs_manager_init(CustomNvsManager* manager) {
    return nvs_manager_init(manager->nvs_common);
}

// 保存自定义数据
esp_err_t custom_nvs_manager_save(CustomNvsManager* manager, const CustomData* data) {
    return nvs_manager_save_blob(manager->nvs_common, manager->data_key, data, sizeof(CustomData));
}

// 加载自定义数据
esp_err_t custom_nvs_manager_load(CustomNvsManager* manager, CustomData* data) {
    return nvs_manager_load_blob(manager->nvs_common, manager->data_key, data, sizeof(CustomData));
}

// 销毁管理器
void custom_nvs_manager_destroy(CustomNvsManager* manager) {
    nvs_manager_destroy(manager->nvs_common);
    free((void*)manager->data_key);
    free(manager);
}
```

### 扩展最佳实践
1. 使用`nvs_manager`接口作为基础构建特定功能
2. 为每种数据类型创建独立的管理模块
3. 始终提供`create`/`init`/`destroy`生命周期函数
4. 使用明确的错误码和日志输出
5. 确保线程安全（需要时添加互斥锁）

## 许可证

本模块遵循与主项目相同的许可证。