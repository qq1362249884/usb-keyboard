# WiFi 应用模块详细技术文档

## 1. 模块概述

本模块实现了ESP32设备的完整WiFi功能管理系统，包括接入点(AP)模式、站点(STA)模式配置、内置HTTP服务器配置界面、WiFi连接状态管理以及与NVS存储系统的集成。通过本模块，用户可以：
- 灵活配置设备作为WiFi客户端(STA)连接到外部WiFi网络
- 设置设备作为WiFi接入点(AP)供其他设备连接并进行配置
- 通过Web页面直观地配置WiFi参数、扫描附近网络
- 管理WiFi连接状态和网络信息
- 安全存储和读取WiFi配置信息到非易失性存储
- 集成键盘映射管理功能，通过Web界面进行配置

## 2. 文件结构与实现机制

### 2.1 目录结构

```
wifi_app/
├── wifi_app.c     # WiFi功能实现的主要代码文件
├── wifi_app.h     # WiFi功能相关的头文件和接口定义
├── index.html     # Web配置界面的HTML文件（内嵌到固件中）
└── README.md      # 模块说明文档（本文件）
```

### 2.2 文件功能详解

#### wifi_app.c
实现了WiFi模块的核心功能，包括：
- WiFi初始化与配置
- HTTP服务器创建与管理
- RESTful API接口实现
- WiFi事件处理机制
- NVS存储交互
- 任务管理与资源释放

#### wifi_app.h
定义了WiFi模块的公共接口、数据类型和全局变量，包括：
- 外部调用函数声明
- 数据结构定义
- 全局变量声明
- 头文件包含

#### index.html
提供了Web配置界面，通过ESP-IDF的嵌入文件机制集成到固件中，实现：
- 设备状态显示
- WiFi网络扫描与连接
- 键盘映射配置与管理
- 响应式设计，支持移动设备访问

## 3. 主要功能模块实现

### 3.1 WiFi 初始化与管理系统

#### 核心实现机制
- **WiFi模式配置**：支持STA、AP、APSTA三种工作模式，通过`esp_wifi_set_mode()`函数实现模式切换
- **NVS存储集成**：使用`WifiNvsManager_t`和`MenuNvsManager_t`管理WiFi配置和状态的持久化存储
- **事件处理系统**：通过`esp_event_handler_instance_register()`注册事件处理程序，处理WiFi和IP相关事件
- **资源生命周期管理**：实现完整的资源分配和释放机制，确保在WiFi开关切换时正确管理资源

#### 关键初始化流程
1. NVS闪存初始化（`nvs_flash_init()`）
2. 创建和初始化NVS管理器实例
3. 创建事件循环（`esp_event_loop_create_default()`）
4. 初始化网络接口（`esp_netif_init()`）
5. 初始化WiFi（`esp_wifi_init()`）
6. 创建AP和STA网络接口
7. 注册事件处理程序
8. 配置WiFi模式和参数
9. 启动WiFi（`esp_wifi_start()`）
10. 启动HTTP服务器

### 3.2 HTTP 服务器与API接口

#### 服务器架构
- 基于ESP-IDF的`esp_http_server`组件实现
- 支持多个URI端点，每个端点对应特定功能的处理函数
- 实现了RESTful风格的API接口

#### 核心URI端点
| URI | 方法 | 功能 | 处理函数 |
|-----|------|------|----------|
| `/` | GET | 返回Web配置页面 | `index_handler` |
| `/favicon.ico` | GET | 返回网站图标 | `favicon_handler` |
| `/connect-wifi` | POST | 连接到指定WiFi网络 | `connect_wifi_handler` |
| `/scan-wifi` | GET | 扫描并返回可用WiFi网络列表 | `scan_wifi_handler` |
| `/get-ip` | GET | 获取当前设备IP地址 | `get_ip_handler` |
| `/load-keymap` | GET | 获取当前键盘映射配置 | `load_keymap_handler` |
| `/save-keymap` | POST | 保存自定义键盘映射配置 | `save_keymap_handler` |

#### 服务器配置特点
- 增加超时设置以提高稳定性（接收/发送超时时间均为5秒）
- 启用LRU缓存清理，优化内存使用
- 支持JSON格式的请求和响应数据

### 3.3 Web配置界面（index.html）

#### 嵌入机制
index.html文件通过ESP-IDF的嵌入文件机制编译到固件中，具体实现：
1. 在`CMakeLists.txt`中通过`EMBED_FILES`指定要嵌入的文件
2. 在代码中通过`extern const uint8_t index_html_start[] asm("_binary_index_html_start")`和`extern const uint8_t index_html_end[] asm("_binary_index_html_end")`引用
3. 通过`httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start)`发送HTML内容

#### 界面功能
- 设备状态显示（WiFi连接状态、IP地址等）
- WiFi网络扫描和列表展示
- WiFi连接参数配置（SSID、密码）
- 键盘映射配置界面
- 响应式设计，适配不同尺寸设备

## 4. 核心API接口详解

### 4.1 公共函数接口

#### wifi_task()
```c
void wifi_task(void);
```
- **功能**：WiFi模块的主任务创建函数，负责初始化和管理WiFi功能
- **内部实现**：通过`xTaskCreate()`创建`app_wifi_task`任务，优先级为4
- **调用时机**：应用程序初始化时调用，或需要重新启动WiFi功能时调用
- **参数**：无
- **返回值**：无

#### wifi_get_mode()
```c
esp_err_t wifi_get_mode(wifi_mode_t *mode);
```
- **功能**：获取当前WiFi工作模式
- **参数**：mode - 用于存储WiFi模式的指针
- **返回值**：ESP_OK表示成功，其他值表示错误

#### wifi_is_connected()
```c
bool wifi_is_connected(void);
```
- **功能**：检查WiFi连接状态
- **实现原理**：通过检查STA接口是否获取到IP地址判断连接状态
- **参数**：无
- **返回值**：true表示已连接，false表示未连接

#### wifi_get_ap_info()
```c
esp_err_t wifi_get_ap_info(char *ssid, size_t ssid_len, char *password, size_t password_len);
```
- **功能**：获取AP模式下的SSID和密码
- **参数**：
  - ssid - 用于存储SSID的缓冲区
  - ssid_len - SSID缓冲区大小
  - password - 用于存储密码的缓冲区
  - password_len - 密码缓冲区大小
- **返回值**：ESP_OK表示成功，其他值表示错误

#### wifi_toggle()
```c
esp_err_t wifi_toggle(bool enable);
```
- **功能**：切换WiFi开关状态
- **内部逻辑**：
  - 启用WiFi时：创建任务（如未创建）、设置模式、启动WiFi和Web服务器
  - 禁用WiFi时：停止Web服务器、停止WiFi、释放资源
- **状态保存**：通过NVS保存WiFi开关状态和模式配置
- **参数**：enable - true启用WiFi，false禁用WiFi
- **返回值**：ESP_OK表示成功，其他值表示错误

#### wifi_get_http_port()
```c
uint16_t wifi_get_http_port(void);
```
- **功能**：获取HTTP服务器端口号
- **实现**：返回预定义常量`HTTP_SERVER_PORT`的值
- **参数**：无
- **返回值**：HTTP服务器端口号

#### wifi_clear_password()
```c
esp_err_t wifi_clear_password(void);
```
- **功能**：清除保存的WiFi密码
- **实现**：通过NVS管理器删除保存的密码信息
- **参数**：无
- **返回值**：ESP_OK表示成功，其他值表示错误

### 4.2 全局变量

```c
extern char client_ip[16];
```
- **功能**：存储客户端IP地址，供其他模块（如oled_menu_display.c）访问
- **更新机制**：在`event_handler`函数中，当获取到IP地址事件发生时更新

## 5. 关键配置与CMake集成

### 5.1 核心配置参数

| 参数名称 | 宏定义 | 默认值 | 说明 |
|----------|--------|--------|------|
| 扫描列表大小 | SCAN_LIST_SIZE | 20 | WiFi扫描结果最大保存数量 |
| AP默认SSID | AP_SSID | "ESP32-AP-Device" | 接入点模式下的默认SSID名称 |
| AP默认密码 | AP_PASSWORD | "123456789" | 接入点模式下的默认密码 |
| HTTP服务器端口 | HTTP_SERVER_PORT | 80 | Web服务器监听端口 |

### 5.2 CMake配置详解

在项目的`CMakeLists.txt`文件中，需要进行以下配置以正确集成WiFi模块和HTML文件：

#### 源文件和头文件目录配置
```cmake
set(src_dirs 
            "."
            keyboard_led
            spi_scanner
            tinyusb_hid
            nvs_manager
            joystick
            ssd1306
            ssd1306/oled_menu
            ssd1306/oled_fonts
            ssd1306/oled_driver
            "../hid_device"
            wifi_app
            init_manager)

set(include_dirs 
            "."
            keyboard_led
            spi_scanner
            tinyusb_hid
            nvs_manager
            init_manager
            joystick
            ssd1306
            ssd1306/oled_menu
            ssd1306/oled_fonts
            ssd1306/oled_driver
            "../hid_device"
            wifi_app)
```

#### 组件依赖配置
```cmake
set(requires esp_timer
             driver
             freertos
             nvs_flash
             esp_http_server
             esp_wifi
             usb
             esp_adc
             )
```

#### HTML文件嵌入配置
```cmake
# 定义需要嵌入到固件中的静态文件
set(embed_files
            ./wifi_app/index.html)

idf_component_register(SRC_DIRS ${src_dirs} 
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES ${requires}
                       EMBED_FILES ${embed_files}
                       )
```

## 6. 完整工作流程

### 6.1 初始化流程详解

1. **任务启动**：调用`wifi_task()`创建并启动`app_wifi_task`任务
2. **NVS初始化**：初始化非易失性存储，为配置保存做准备
3. **管理器创建**：创建并初始化WiFi NVS管理器和菜单NVS管理器
4. **事件循环**：创建默认事件循环，用于处理WiFi和IP事件
5. **网络接口**：初始化网络接口并创建AP和STA接口实例
6. **事件注册**：注册WiFi和IP事件处理程序
7. **模式配置**：根据保存的配置或默认值设置WiFi工作模式
8. **参数设置**：配置AP模式参数（SSID、密码、信道、最大连接数等）
9. **WiFi启动**：调用`esp_wifi_start()`启动WiFi功能
10. **服务器启动**：启动HTTP服务器并注册URI处理程序
11. **自动连接**：如果配置了STA模式且有保存的WiFi信息，则自动尝试连接

### 6.2 WiFi事件处理流程

模块通过`event_handler`函数处理以下主要事件：

| 事件类型 | 处理逻辑 |
|----------|----------|
| WIFI_EVENT_STA_START | STA模式启动，记录日志 |
| WIFI_EVENT_STA_CONNECTED | 连接到WiFi接入点，记录日志 |
| WIFI_EVENT_STA_DISCONNECTED | 从WiFi接入点断开连接，记录原因，可选择自动切换到AP模式 |
| IP_EVENT_STA_GOT_IP | 获取到IP地址，更新client_ip变量，记录日志 |
| WIFI_EVENT_AP_START | AP模式启动，记录日志 |
| WIFI_EVENT_AP_STOP | AP模式停止，记录日志 |
| WIFI_EVENT_AP_STACONNECTED | 客户端连接到AP，记录客户端MAC地址和连接数 |
| WIFI_EVENT_AP_STADISCONNECTED | 客户端从AP断开连接，记录客户端MAC地址和剩余连接数 |

### 6.3 HTTP请求处理流程

1. **请求接收**：HTTP服务器接收来自客户端的请求
2. **URI匹配**：根据请求的URI匹配对应的处理函数
3. **参数解析**：对于POST请求，解析请求体中的参数
4. **业务处理**：执行相应的业务逻辑（如WiFi连接、扫描等）
5. **响应生成**：生成JSON格式的响应数据
6. **响应发送**：将响应发送回客户端

## 7. 高级功能与实现细节

### 7.1 WiFi模式智能切换

当STA模式连接失败时，系统会自动切换到AP模式，提供Web配置界面，实现了无缝的用户体验。这一功能通过事件处理函数中的重连逻辑实现。

### 7.2 WiFi扫描结果处理

WiFi扫描功能不仅返回可用网络列表，还实现了以下高级特性：
- 扫描结果去重处理
- 按信号强度排序
- 限制最大返回数量（由SCAN_LIST_SIZE控制）

### 7.3 内存优化策略

- 启用HTTP服务器的LRU缓存清理功能
- 合理管理动态内存分配和释放
- 固定大小的缓冲区设计，避免内存碎片

### 7.4 错误处理与恢复机制

- 完善的错误检查和日志记录
- 资源释放的安全保障
- 失败重试机制
- 异常状态下的优雅降级

## 8. 正确使用示例

### 8.1 启动WiFi功能

```c
// 在应用程序初始化时启动WiFi任务
wifi_task();
```

> 注意：该函数内部会调用`xTaskCreate()`创建名为`app_wifi_task`的任务，优先级为4，堆栈大小为4*1024字节。不需要直接使用`xTaskCreatePinnedToCore`调用`wifi_task`函数。

### 8.2 获取WiFi连接状态

```c
if (wifi_is_connected()) {
    ESP_LOGI(TAG, "WiFi已连接");
    // 执行需要网络连接的操作
} else {
    ESP_LOGI(TAG, "WiFi未连接");
    // 执行离线模式操作或提示用户连接WiFi
}
```

### 8.3 切换WiFi开关

```c
// 启用WiFi
esp_err_t err = wifi_toggle(true);
if (err != ESP_OK) {
    ESP_LOGE(TAG, "启用WiFi失败: %s", esp_err_to_name(err));
}

// 禁用WiFi以节省电量
err = wifi_toggle(false);
if (err != ESP_OK) {
    ESP_LOGE(TAG, "禁用WiFi失败: %s", esp_err_to_name(err));
}
```

### 8.4 获取AP模式配置信息

```c
char ssid[32];
char password[64];
if (wifi_get_ap_info(ssid, sizeof(ssid), password, sizeof(password)) == ESP_OK) {
    ESP_LOGI(TAG, "AP模式信息 - SSID: %s, 密码: %s", ssid, password);
}
```

### 8.5 清除保存的WiFi密码

```c
if (wifi_clear_password() == ESP_OK) {
    ESP_LOGI(TAG, "WiFi密码已清除");
}
```

## 9. 注意事项与最佳实践

1. **NVS初始化**：确保在使用WiFi功能前，系统已经正确初始化NVS闪存
2. **电源管理**：在不需要WiFi功能时，务必调用`wifi_toggle(false)`关闭WiFi以节省电量
3. **配置更新**：修改WiFi配置后，建议调用`wifi_toggle(false)`和`wifi_toggle(true)`重启WiFi使配置生效
4. **自动模式切换**：当作为STA模式连接失败时，设备会自动切换到AP模式，提供配置界面
5. **事件处理**：事件处理函数中已实现对事件处理程序的正确注册和注销，避免重复打印日志
6. **内存管理**：注意管理好传入API的缓冲区大小，避免缓冲区溢出
7. **错误处理**：所有API调用都应检查返回值，确保正确处理错误情况

## 10. 常见问题排查

### 10.1 WiFi连接失败
- 检查SSID和密码是否正确，注意大小写和特殊字符
- 确认WiFi信号强度是否足够，尝试靠近路由器
- 检查NVS存储是否正常工作，可尝试擦除NVS后重新配置
- 确认目标WiFi网络支持的认证方式是否与设备兼容

### 10.2 Web配置页面无法访问
- 确认设备是否已启动AP模式（默认SSID为"ESP32-AP-Device"）
- 检查设备IP地址是否正确（AP模式下默认为192.168.4.1）
- 确认HTTP服务器是否正常运行（检查日志输出）
- 尝试清除浏览器缓存或使用其他浏览器访问

### 10.3 WiFi功能异常耗电
- 在不需要WiFi功能时，确保调用`wifi_toggle(false)`关闭WiFi
- 合理设置WiFi休眠模式和电源管理策略
- 检查是否存在异常的WiFi重连循环

### 10.4 配置信息丢失
- 检查NVS存储是否正常工作
- 确认设备是否经历了意外断电或重置
- 对于重要配置，建议实现备份机制

## 11. 扩展开发指南

### 11.1 添加新的API端点

1. 实现新的处理函数，遵循`esp_err_t handler(httpd_req_t *req)`格式
2. 定义新的URI配置结构体，如：
   ```c
   static const httpd_uri_t new_uri = {
       .uri       = "/new-endpoint",
       .method    = HTTP_GET,
       .handler   = new_handler,
       .user_ctx  = NULL
   };
   ```
3. 在`start_webserver()`函数中注册新的URI处理程序：
   ```c
   httpd_register_uri_handler(server, &new_uri);
   ```

### 11.2 修改Web配置界面

1. 编辑`index.html`文件，修改界面布局或功能
2. 重新编译项目，使修改生效
3. 对于较大的更改，建议先在浏览器中测试HTML文件

### 11.3 调整WiFi参数

可以通过修改`wifi_app.c`文件中的以下宏定义来调整WiFi参数：
```c
#define SCAN_LIST_SIZE 20          // 扫描列表大小
#define AP_SSID "ESP32-AP-Device"  // 默认AP名称
#define AP_PASSWORD "123456789"     // 默认AP密码
#define HTTP_SERVER_PORT 80        // HTTP服务器端口
```