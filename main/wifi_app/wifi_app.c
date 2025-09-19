#include "wifi_app.h"


#include <time.h>

// 日志标签
#define TAG "wifi_app"

// 配置常量
#define SCAN_LIST_SIZE 20          // 扫描列表大小
#define AP_SSID "ESP32-AP-Device"  // 默认AP名称
#define AP_PASSWORD "123456789"     // 默认AP密码
#define HTTP_SERVER_PORT 80        // HTTP服务器端口
#define AP_CLIENT_TIMEOUT 180      // AP无客户端自动关闭时间(秒) - 3分钟

// 全局变量
esp_netif_t *sta_netif = NULL; // STA网络接口句柄
esp_netif_t *ap_netif = NULL;  // AP网络接口句柄
char client_ip[16] = {0};       // 客户端IP地址
httpd_handle_t server = NULL;   // HTTP服务器句柄

// WiFi状态变量
wifi_mode_t saved_wifi_mode = WIFI_MODE_NULL;  // 保存的WiFi模式
TaskHandle_t wifi_task_handle = NULL;           // WiFi任务句柄

// 自动关闭相关变量
bool ap_has_clients = false;     // AP是否有客户端连接
bool sta_connected_flag = false; // STA是否已连接
int ap_client_count = 0;         // AP客户端数量
time_t last_ap_client_time = 0;  // 上次有AP客户端连接的时间
time_t wifi_start_time = 0;      // WiFi启动时间
bool wifi_manually_stopped = false; // WiFi是否被主动关闭

// NVS管理器实例
static WifiNvsManager_t* wifi_nvs_manager = NULL;
static MenuNvsManager_t* menu_nvs_manager = NULL;

// 事件处理程序实例句柄
static esp_event_handler_instance_t wifi_event_handler_instance = NULL;
static esp_event_handler_instance_t ip_event_handler_instance = NULL;

// HTML文件引用
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

/**
 * @brief 处理根路径(/)的HTTP请求，返回HTML页面
 */
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

/**
 * @brief 处理favicon.ico请求
 */
static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, NULL, 0); // 返回空图标
    return ESP_OK;
}

/**
 * @brief 连接WiFi接口处理函数
 * 处理POST请求，解析SSID和密码并尝试连接WiFi
 */
static esp_err_t connect_wifi_handler(httpd_req_t *req)
{
    char buf[100];
    ssize_t len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        if (len == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[len] = '\0';

    // 解析JSON格式的请求数据
    char ssid[32] = {0};
    char password[64] = {0};
    
    // 查找SSID字段
    char *ssid_start = strstr(buf, "\"ssid\":\"");
    char *ssid_end = NULL;
    if (ssid_start) {
        ssid_start += strlen("\"ssid\":\"");
        ssid_end = strstr(ssid_start, "\"");
        if (ssid_end) {
            strncpy(ssid, ssid_start, (ssid_end - ssid_start) < (sizeof(ssid) - 1) ? (ssid_end - ssid_start) : (sizeof(ssid) - 1));
        }
    }
    
    // 查找password字段
    char *password_start = strstr(buf, "\"password\":\"");
    char *password_end = NULL;
    if (password_start) {
        password_start += strlen("\"password\":\"");
        password_end = strstr(password_start, "\"");
        if (password_end) {
            strncpy(password, password_start, (password_end - password_start) < (sizeof(password) - 1) ? (password_end - password_start) : (sizeof(password) - 1));
        }
    }
    
    // 验证SSID和密码
    if (strlen(ssid) == 0 || strlen(password) == 0) {
        const char* resp = "{\"status\":\"error\",\"message\":\"无效的SSID或密码\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    // 保存WiFi配置到NVS
    if (wifi_nvs_manager) {
        esp_err_t nvs_ret = wifi_nvs_manager_save_config(wifi_nvs_manager, ssid, password);
        if (nvs_ret != ESP_OK) {
            ESP_LOGE(TAG, "保存WiFi配置失败: %s", esp_err_to_name(nvs_ret));
        } else {
            ESP_LOGI(TAG, "WiFi配置保存成功 - SSID: %s", ssid);
        }
    } else {
        ESP_LOGE(TAG, "WiFi NVS管理器未初始化，无法保存配置");
    }
    
    // 配置并连接WiFi
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_err_t ret = esp_wifi_connect();
    
    if (ret == ESP_OK) {
        const char* resp = "{\"status\":\"success\",\"message\":\"连接请求已发送，正在尝试连接...\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    } else {
        const char* resp_format = "{\"status\":\"error\",\"message\":\"连接请求失败: %s\"}";
        char error_resp[100];
        snprintf(error_resp, sizeof(error_resp), resp_format, esp_err_to_name(ret));
        httpd_resp_send(req, error_resp, HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

/**
 * @brief 扫描WiFi列表接口处理函数
 * 执行WiFi扫描并返回可用SSID列表
 */
static esp_err_t scan_wifi_handler(httpd_req_t *req)
{
    // 定义唯一AP结构体
    typedef struct { char ssid[33]; int8_t rssi; } UniqueAP;
    UniqueAP unique_aps[SCAN_LIST_SIZE] = {0};
    int unique_count = 0;
    
    // 配置扫描参数
    wifi_scan_config_t scan_config = {
        .show_hidden = true
    };

    // 开始扫描
    esp_err_t scan_ret = esp_wifi_scan_start(&scan_config, true);
    if (scan_ret != ESP_OK) {
        ESP_LOGE(TAG, "扫描WiFi失败: %s", esp_err_to_name(scan_ret));
        const char* resp = "{\"status\":\"error\",\"message\":\"扫描WiFi失败\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    // 获取扫描结果
    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    // 限制最大返回数量
    if (ap_count > SCAN_LIST_SIZE) {
        ap_count = SCAN_LIST_SIZE;
    }
    
    // 分配内存存储扫描结果
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_records) {
        ESP_LOGE(TAG, "内存分配失败");
        const char* resp = "{\"status\":\"error\",\"message\":\"内存分配失败\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    // 获取扫描记录并去重
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));
    for (int i = 0; i < ap_count; i++) {
        // 跳过空白SSID
        if (strlen((char*)ap_records[i].ssid) == 0) continue;

        bool is_duplicate = false;
        // 检查是否重复并保留信号最强的
        for (int j = 0; j < unique_count; j++) {
            if (strcmp((char*)ap_records[i].ssid, unique_aps[j].ssid) == 0) {
                is_duplicate = true;
                // 保留信号更强的（RSSI值更大）
                if (ap_records[i].rssi > unique_aps[j].rssi) {
                    unique_aps[j].rssi = ap_records[i].rssi;
                }
                break;
            }
        }

        // 如果不是重复项则添加到列表
        if (!is_duplicate && unique_count < SCAN_LIST_SIZE) {
            strncpy(unique_aps[unique_count].ssid, (char*)ap_records[i].ssid, sizeof(unique_aps[unique_count].ssid) - 1);
            unique_aps[unique_count].rssi = ap_records[i].rssi;
            unique_count++;
        }
    }

    // 按信号强度排序（从强到弱）
    for (int i = 0; i < unique_count; i++) {
        for (int j = i + 1; j < unique_count; j++) {
            if (unique_aps[j].rssi > unique_aps[i].rssi) {
                UniqueAP temp = unique_aps[i];
                unique_aps[i] = unique_aps[j];
                unique_aps[j] = temp;
            }
        }
    }
    
    ESP_LOGI(TAG, "扫描到的AP数量: %d (去重后: %d)", ap_count, unique_count);
    
    // 构建JSON响应
    char resp[512] = "{\"status\":\"success\",\"data\":[";
    for (int i = 0; i < unique_count; i++) {
        if (i > 0) strcat(resp, ",");
        strcat(resp, "\"");
        strcat(resp, unique_aps[i].ssid);
        strcat(resp, "\"");
    }
    strcat(resp, "]}");

    free(ap_records);
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief 获取当前IP接口处理函数
 * 返回设备当前IP地址
 */
static esp_err_t get_ip_handler(httpd_req_t *req)
{
    // 获取当前WiFi模式
    wifi_mode_t current_mode;
    esp_err_t mode_ret = esp_wifi_get_mode(&current_mode);
    if (mode_ret != ESP_OK) {
        const char *resp = "{\"status\":\"error\",\"message\":\"Failed to get WiFi mode\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return mode_ret;
    }

    // 使用全局网络接口句柄
    esp_netif_t *netif = NULL;
    ESP_LOGI(TAG, "Current WiFi mode: %d", current_mode);
    
    // 对于APSTA模式，优先使用STA接口（如果已连接）
    if (current_mode & WIFI_MODE_STA) {
        // 检查STA是否已连接
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            netif = sta_netif;
            ESP_LOGI(TAG, "Using connected STA interface");
        } else if (current_mode & WIFI_MODE_AP) {
            netif = ap_netif;
            ESP_LOGI(TAG, "Using AP interface (STA not connected)");
        }
    } else if (current_mode & WIFI_MODE_AP) {
        netif = ap_netif;
        ESP_LOGI(TAG, "Using AP interface");
    }

    // 检查接口是否有效
    if (!netif) {
        const char *resp = "{\"status\":\"error\",\"message\":\"Network interface not available\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    // 获取IP信息
    esp_netif_ip_info_t ip_info;
    memset(&ip_info, 0, sizeof(ip_info));
    esp_err_t ret = esp_netif_get_ip_info(netif, &ip_info);
    if (ret != ESP_OK) {
        const char *resp = "{\"status\":\"error\",\"message\":\"Failed to get IP info\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ret;
    }
    
    // 检查IP地址是否有效
    if (ip_info.ip.addr == 0) {
        const char *resp = "{\"status\":\"error\",\"message\":\"IP address not assigned yet\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    char ip[16];
    if (netif == ap_netif) {
        // AP模式下返回已连接客户端IP
        if (strlen(client_ip) == 0) {
            const char *resp = "{\"status\":\"error\",\"message\":\"No client connected to AP\"}";
            httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        snprintf(ip, sizeof(ip), "%s", client_ip);
    } else {
        // STA模式下返回设备自身IP
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ip_info.ip));
    }
    
    char resp[200];
    snprintf(resp, sizeof(resp), "{\"status\":\"success\",\"ip\":\"%s\"}", ip);
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    
    return ESP_OK;
}

/**
 * @brief 加载自定义按键映射接口处理函数
 * 返回当前保存的自定义按键映射
 */
static esp_err_t load_keymap_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    
    // 加载自定义映射（层1）
    uint16_t keymap[NUM_KEYS] = {0};
    esp_err_t err = load_keymap_from_nvs(1, keymap);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load keymap: %s", esp_err_to_name(err));
        const char* resp = "{\"status\":\"error\",\"message\":\"加载按键映射失败\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    // 构建JSON响应
    char resp[512] = "{\"status\":\"success\",\"keymap\":[";
    for (int i = 0; i < NUM_KEYS; i++) {
        if (i > 0) strcat(resp, ",");
        char code_str[10];
        sprintf(code_str, "%d", keymap[i]);
        strcat(resp, code_str);
    }
    strcat(resp, "]}");
    
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief 保存自定义按键映射接口处理函数
 * 保存用户配置的自定义按键映射
 */
static esp_err_t save_keymap_handler(httpd_req_t *req)
{
    char buf[512];
    ssize_t len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        if (len == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[len] = '\0';
    
    // 解析JSON格式的请求数据
    uint16_t keymap[NUM_KEYS] = {0};
    char *keymap_start = strstr(buf, "\"keymap\":[");
    
    if (keymap_start) {
        keymap_start += strlen("\"keymap\":[");
        char *keymap_end = strstr(keymap_start, "]");
        
        if (keymap_end) {
            *keymap_end = '\0'; // 临时修改字符串以提取keymap数组
            
            // 解析数组元素
            char *token = strtok(keymap_start, ",");
            int index = 0;
            
            while (token != NULL && index < NUM_KEYS) {
                keymap[index] = atoi(token);
                token = strtok(NULL, ",");
                index++;
            }
            
            *keymap_end = ']'; // 恢复原始字符串
        }
    }
    
    // 保存自定义映射（层1）
    esp_err_t err = save_keymap_to_nvs(1, keymap);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save keymap: %s", esp_err_to_name(err));
        const char* resp = "{\"status\":\"error\",\"message\":\"保存按键映射失败\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    const char* resp = "{\"status\":\"success\",\"message\":\"按键映射保存成功\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// HTTP服务器URI配置
static const httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t favicon_uri = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = favicon_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t connect_wifi_uri = {
    .uri       = "/connect-wifi",
    .method    = HTTP_POST,
    .handler   = connect_wifi_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t scan_wifi_uri = {
    .uri       = "/scan-wifi",
    .method    = HTTP_GET,
    .handler   = scan_wifi_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t get_ip_uri = {
    .uri       = "/get-ip",
    .method    = HTTP_GET,
    .handler   = get_ip_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t load_keymap_uri = {
    .uri       = "/load-keymap",
    .method    = HTTP_GET,
    .handler   = load_keymap_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t save_keymap_uri = {
    .uri       = "/save-keymap",
    .method    = HTTP_POST,
    .handler   = save_keymap_handler,
    .user_ctx  = NULL
};

/**
 * @brief 启动HTTP服务器并注册URI处理程序
 * @return ESP_OK表示服务器启动成功
 */
static esp_err_t start_webserver(void)
{
    // 如果服务器已在运行，先停止它
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "已停止现有HTTP服务器实例");
    }

    // 定义HTTP服务器配置
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // 增加超时设置以提高服务器稳定性，减少连接重置错误
    config.recv_wait_timeout = 5; // 增加接收超时时间(秒)
    config.send_wait_timeout = 5; // 增加发送超时时间(秒)
    config.lru_purge_enable = true; // 启用LRU缓存清理
    
    esp_err_t start_ret = httpd_start(&server, &config);

    if (start_ret == ESP_OK) {
        // 注册URI处理程序
        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &favicon_uri);
        httpd_register_uri_handler(server, &connect_wifi_uri);
        httpd_register_uri_handler(server, &scan_wifi_uri);
        httpd_register_uri_handler(server, &get_ip_uri);
        httpd_register_uri_handler(server, &load_keymap_uri);
        httpd_register_uri_handler(server, &save_keymap_uri);
        ESP_LOGI(TAG, "HTTP服务器启动成功");
    } else {
        ESP_LOGE(TAG, "HTTP服务器启动失败: %s", esp_err_to_name(start_ret));
        server = NULL;
    }

    return start_ret;
}

/**
 * @brief 停止HTTP服务器
 */
static void stop_webserver(void)
{
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "HTTP服务器已停止");
    }
}

/**
 * @brief 从NVS存储中读取WiFi配置信息
 * @param ssid 用于存储读取到的SSID的缓冲区
 * @param password 用于存储读取到的密码的缓冲区
 * @return ESP_OK表示读取成功
 */
static esp_err_t read_wifi_config(char *ssid, char *password)
{
    // 检查WiFi NVS管理器是否已初始化
    if (!wifi_nvs_manager) {
        ESP_LOGE(TAG, "WiFi NVS管理器未初始化");
        return ESP_FAIL;
    }

    // 检查配置是否存在
    if (!wifi_nvs_manager_has_config(wifi_nvs_manager)) {
        ESP_LOGW(TAG, "WiFi配置不存在");
        return ESP_ERR_NVS_NOT_FOUND;
    }

    // 读取WiFi配置
    esp_err_t ret = wifi_nvs_manager_load_config(wifi_nvs_manager, 
                                               ssid, 
                                               32, 
                                               password, 
                                               64);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "读取WiFi配置成功 - SSID: %s", ssid);
    } else {
        ESP_LOGW(TAG, "读取WiFi配置失败: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief WiFi和IP事件处理函数
 */
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch(event_id)
        {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
                // 获取断开原因并打印
                wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*)event_data;
                ESP_LOGI(TAG, "Disconnected reason: %d", disconnected->reason);
                sta_connected_flag = false;
                
                // 先检查WiFi是否是被主动关闭的，如果是则直接返回
                if (wifi_manually_stopped) {
                    ESP_LOGI(TAG, "WiFi是主动关闭的，不进行重连");
                    break;
                }
                
                // 添加断开连接后的重连逻辑
                char ssid[32];
                char password[64];
                
                if (read_wifi_config(ssid, password) == ESP_OK) {
                    wifi_mode_t current_mode;
                    if (esp_wifi_get_mode(&current_mode) == ESP_OK) {
                        if (current_mode == WIFI_MODE_STA || current_mode == WIFI_MODE_APSTA) {
                            // 检查WiFi是否处于运行状态
                            if (current_mode != WIFI_MODE_NULL) {
                                ESP_LOGI(TAG, "尝试重新连接WiFi");
                                // 配置并尝试重新连接WiFi
                                wifi_config_t wifi_sta_config = {
                                    .sta = {
                                        .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
                                        .pmf_cfg = {
                                            .capable = true,
                                            .required = false
                                        }
                                    },
                                };
                                memcpy(&wifi_sta_config.sta.ssid, ssid, sizeof(ssid));
                                memcpy(&wifi_sta_config.sta.password, password, sizeof(password));
                                  
                                esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
                                if (err == ESP_OK) {
                                    esp_wifi_connect();
                                } else {
                                    ESP_LOGW(TAG, "设置WiFi配置失败: %s", esp_err_to_name(err));
                                }
                            }
                        }
                    }
                }
                break;
            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "WIFI_EVENT_SCAN_DONE");
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
                ap_client_count++;
                ap_has_clients = true;
                last_ap_client_time = time(NULL);
                ESP_LOGI(TAG, "AP客户端数量: %d", ap_client_count);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d", MAC2STR(event->mac), event->aid, event->reason);
                if (ap_client_count > 0) {
                    ap_client_count--;
                }
                ap_has_clients = (ap_client_count > 0);
                if (!ap_has_clients) {
                    last_ap_client_time = time(NULL);
                    ESP_LOGI(TAG, "最后一个AP客户端离开，开始计时");
                }
                ESP_LOGI(TAG, "AP客户端数量: %d", ap_client_count);
                // 清除客户端IP地址
                memset(client_ip, 0, sizeof(client_ip));
                ESP_LOGI(TAG, "客户端IP已清除");
                break;
            }
            default:
                ESP_LOGI(TAG, "其他WIFI_EVENT: %" PRIu32, event_id);
                break;
        }
    }
    else if(event_base == IP_EVENT)
    {
        switch(event_id)
        {
            case IP_EVENT_STA_GOT_IP: {                
                ip_event_got_ip_t* event_data_ptr = (ip_event_got_ip_t*) event_data;
                char ip_str[16];
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event_data_ptr->ip_info.ip));
                ESP_LOGI(TAG, "设备已连接到AP，获取IP: %s", ip_str);
                
                strncpy(client_ip, ip_str, sizeof(client_ip) - 1);
                ESP_LOGI(TAG, "Web服务器地址已保存: %s", client_ip);
                
                sta_connected_flag = true;
                
                // 关闭AP模式
                ESP_LOGI(TAG, "STA连接成功后禁用AP模式");
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
                
                break;
            }
            case IP_EVENT_AP_STAIPASSIGNED: {
                // 保存AP设备本身的IP地址192.168.4.1
                snprintf(client_ip, sizeof(client_ip), "192.168.4.1");
                ESP_LOGI(TAG, "AP IP地址: %s", client_ip);
                break;
            }
            default:
                ESP_LOGI(TAG, "其他IP_EVENT: %" PRIu32, event_id);
                break;
        }
    }
}

/**
 * @brief WiFi任务主函数
 * 负责WiFi初始化和管理，包括AP+STA模式设置、连接管理和Web服务器
 */
static void app_wifi_task(void *pvParameters)
{
    // 局部变量
    char sta_ssid[32] = {0};
    char sta_password[64] = {0};
    
    // WiFi初始化配置
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 创建和初始化WiFi NVS管理器
    wifi_nvs_manager = wifi_nvs_manager_create();
    if (wifi_nvs_manager) {
        ret = wifi_nvs_manager_init(wifi_nvs_manager);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "初始化WiFi NVS管理器失败: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "创建WiFi NVS管理器失败");
    }
    
    // 创建并初始化菜单NVS管理器，用于保存WiFi开关状态
    menu_nvs_manager = menu_nvs_manager_create(NULL, 0, false);
    if (menu_nvs_manager) {
        ret = menu_nvs_manager_init(menu_nvs_manager);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "初始化菜单NVS管理器失败: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "创建菜单NVS管理器失败");
    }

    // 创建事件循环，但避免重复创建
    esp_err_t event_loop_ret = esp_event_loop_create_default();
    if (event_loop_ret != ESP_OK && event_loop_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "创建事件循环失败: %s", esp_err_to_name(event_loop_ret));
    }

    ESP_ERROR_CHECK(esp_netif_init());

    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 创建网络接口
    ap_netif  = esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();

    // 注册事件处理程序
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &wifi_event_handler_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &ip_event_handler_instance));

    // 检查之前是否保存了WiFi模式，如果有则使用该模式
    if (saved_wifi_mode != WIFI_MODE_NULL) {
        ESP_LOGI(TAG, "使用保存的WiFi模式: %d", saved_wifi_mode);
        ESP_ERROR_CHECK(esp_wifi_set_mode(saved_wifi_mode));
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    }

    // 配置AP模式参数
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = 1,
            .password = AP_PASSWORD,
            .max_connection = 2,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(500)); // 等待初始化完成

    // 如果WiFi模式包含STA模式，并且有保存的WiFi配置，则尝试连接
    wifi_mode_t current_mode;
    if (esp_wifi_get_mode(&current_mode) == ESP_OK) {
        if (((current_mode & WIFI_MODE_STA) || (current_mode == WIFI_MODE_APSTA)) && 
            read_wifi_config(sta_ssid, sta_password) == ESP_OK) {
            ESP_LOGI(TAG, "读取wifi_SSID: %s", sta_ssid);
            
            // 配置并连接WiFi
            wifi_config_t wifi_sta_config = {
                .sta = {
                    .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
                    .pmf_cfg = {
                        .capable = true,
                        .required = false
                    }
                },
            };
            memcpy(&wifi_sta_config.sta.ssid, sta_ssid, sizeof(sta_ssid));
            memcpy(&wifi_sta_config.sta.password, sta_password, sizeof(sta_password));
            
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
            
            // 添加重连机制：尝试连接3次，每次间隔10秒
            const int max_retry = 3;
            const int retry_delay = 10000; // 10秒
            int retry_count = 0;
            bool connected = false;
            
            while (retry_count < max_retry && !connected) {
                ESP_LOGI(TAG, "尝试连接WiFi (第 %d/%d 次)", retry_count + 1, max_retry);
                esp_err_t conn_ret = esp_wifi_connect();
                
                if (conn_ret == ESP_OK) {
                    // 等待连接结果，最多等待5秒
                    for (int i = 0; i < 50; i++) { // 50 * 100ms = 5秒
                        vTaskDelay(pdMS_TO_TICKS(100));
                        
                        // 检查是否已连接
                        wifi_ap_record_t ap_info;
                        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                            connected = true;
                            ESP_LOGI(TAG, "WiFi连接成功");
                            break;
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "连接请求失败: %s", esp_err_to_name(conn_ret));
                }
                
                if (!connected && retry_count < max_retry - 1) {
                    ESP_LOGI(TAG, "连接失败，%d秒后重试...", retry_delay / 1000);
                    vTaskDelay(pdMS_TO_TICKS(retry_delay));
                }
                
                retry_count++;
            }
            
            if (!connected) {
                ESP_LOGW(TAG, "已尝试%d次连接失败，保持APSTA模式", max_retry);
                // 保持在APSTA模式，让用户可以通过Web界面重新配置
            }
        }
    }
    
    // 记录WiFi启动时间
    wifi_start_time = time(NULL);
    last_ap_client_time = time(NULL);
    ap_client_count = 0;
    ap_has_clients = false;
    sta_connected_flag = false;
    wifi_manually_stopped = false;

    // WiFi模式确定后再启动Web服务器
    wifi_mode_t final_mode;
    if (esp_wifi_get_mode(&final_mode) == ESP_OK && final_mode != WIFI_MODE_NULL) {
        ESP_LOGI(TAG, "WiFi模式已确定为: %d，启动Web服务器", final_mode);
        ESP_ERROR_CHECK(start_webserver());
    } else {
        ESP_LOGI(TAG, "WiFi模式为NULL，不启动Web服务器");
    }

    // WiFi任务主循环
    while (1) {
        // 检查WiFi状态，当WiFi被停止时退出循环
        wifi_mode_t current_mode;
        if (esp_wifi_get_mode(&current_mode) == ESP_OK && current_mode == WIFI_MODE_NULL) {
            ESP_LOGI(TAG, "检测到WiFi已停止，准备退出任务");
            break;
        }
        
        // 检查是否需要自动关闭WiFi
        time_t current_time = time(NULL);
        bool should_auto_off = false;
        
        // 仅在APSTA模式下，且AP无人连接且STA连接不上时才自动关闭WiFi
        if (current_mode == WIFI_MODE_APSTA) {
            // 检查两个条件：
            // 1. STA未连接
            // 2. AP无客户端且超过设定时间
            if (!sta_connected_flag && 
                !ap_has_clients && 
                (current_time - last_ap_client_time) >= AP_CLIENT_TIMEOUT) {
                should_auto_off = true;
                ESP_LOGI(TAG, "APSTA模式下长时间无客户端且STA未连接，准备关闭WiFi");
            }
        }
           
        // 如果满足自动关闭条件，关闭WiFi
        if (should_auto_off) {
            ESP_LOGI(TAG, "执行WiFi自动关闭");
            
            // 标记WiFi是主动关闭的，避免重连
            wifi_manually_stopped = true;
            
            // 保存WiFi状态到NVS
            if (menu_nvs_manager) {
                menu_nvs_manager_save_wifi_state(menu_nvs_manager, false);
            }
            
            // 停止Web服务器
            stop_webserver();
            
            // 设置WiFi模式为NULL
            esp_wifi_set_mode(WIFI_MODE_NULL);
            
            // 停止WiFi
            esp_wifi_stop();
            
            ESP_LOGI(TAG, "WiFi已自动关闭");
        }
        
        // 每5秒检查一次
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    // 释放WiFi NVS管理器资源
    if (wifi_nvs_manager) {
        wifi_nvs_manager_destroy(wifi_nvs_manager);
        wifi_nvs_manager = NULL;
    }
    
    // 清除任务句柄，允许重新创建任务
    wifi_task_handle = NULL;
    
    ESP_LOGI(TAG, "WiFi任务已退出");
    vTaskDelete(NULL);
}

/**
 * @brief 完全释放WiFi资源
 * @return ESP_OK表示成功
 */
static esp_err_t wifi_release_resources(void)
{
    esp_err_t ret = ESP_OK;
    
    // 停止Web服务器
    stop_webserver();
    
    // 标记WiFi是主动关闭的，避免重连
    wifi_manually_stopped = true;
    
    // 停止WiFi
    ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "停止WiFi失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 注销事件处理程序
    if (wifi_event_handler_instance != NULL) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_instance);
        wifi_event_handler_instance = NULL;
    }
    if (ip_event_handler_instance != NULL) {
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler_instance);
        ip_event_handler_instance = NULL;
    }
    
    // 保存当前WiFi模式
    esp_wifi_get_mode(&saved_wifi_mode);
    
    // 设置WiFi模式为NULL
    ret = esp_wifi_set_mode(WIFI_MODE_NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置WiFi模式为NULL失败: %s", esp_err_to_name(ret));
    }
    
    // 释放网络接口资源（如果存在）
    if (ap_netif != NULL) {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }
    if (sta_netif != NULL) {
        esp_netif_destroy(sta_netif);
        sta_netif = NULL;
    }
    
    // 清除客户端IP地址
    memset(client_ip, 0, sizeof(client_ip));
    
    return ret;
}

/**
 * @brief 切换WiFi开关
 * @param enable true表示启用WiFi，false表示禁用WiFi
 * @return ESP_OK表示成功
 */
esp_err_t wifi_toggle(bool enable)
{
    if (enable) {
        // 如果WiFi任务未创建，先创建任务
        if (wifi_task_handle == NULL) {
            ESP_LOGI(TAG, "WiFi任务不存在，创建新的WiFi任务");
            wifi_task();
            
            // 保存WiFi开启状态和当前模式
            if (menu_nvs_manager) {
                menu_nvs_manager_save_wifi_state(menu_nvs_manager, true);
                
                // 获取并保存当前WiFi模式
                wifi_mode_t current_mode;
                if (esp_wifi_get_mode(&current_mode) == ESP_OK) {
                    saved_wifi_mode = current_mode;
                    menu_nvs_manager_save_wifi_mode(menu_nvs_manager, saved_wifi_mode);
                }
            }
            return ESP_OK;
        }
        
        // 检查当前WiFi模式
        wifi_mode_t current_mode;
        esp_err_t ret = esp_wifi_get_mode(&current_mode);
        if (ret != ESP_OK) {
            // 如果获取模式失败，重新创建WiFi任务进行完整初始化
            ESP_LOGE(TAG, "获取WiFi模式失败，重新创建WiFi任务: %s", esp_err_to_name(ret));
            
            // 首先确保旧任务已停止
            if (wifi_release_resources() != ESP_OK) {
                ESP_LOGE(TAG, "释放WiFi资源失败");
            }
            
            // 重新创建WiFi任务
            wifi_task();
            
            // 保存WiFi开启状态和当前模式
            if (menu_nvs_manager) {
                menu_nvs_manager_save_wifi_state(menu_nvs_manager, true);
                
                // 获取并保存当前WiFi模式
                if (esp_wifi_get_mode(&current_mode) == ESP_OK) {
                    saved_wifi_mode = current_mode;
                    menu_nvs_manager_save_wifi_mode(menu_nvs_manager, saved_wifi_mode);
                }
            }
            return ESP_OK;
        }
        
        // 如果WiFi已停止，重新启动WiFi
        if (current_mode == WIFI_MODE_NULL) {
            ESP_LOGI(TAG, "WiFi已停止，重新启动WiFi");
            
            // 配置WiFi模式
            if (saved_wifi_mode == WIFI_MODE_NULL) {
                // 如果是首次启动，设置为AP+STA模式
                ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "设置WiFi模式失败: %s", esp_err_to_name(ret));
                    return ret;
                }
            } else {
                // 设置为之前保存的模式
                ret = esp_wifi_set_mode(saved_wifi_mode);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "恢复WiFi模式失败: %s", esp_err_to_name(ret));
                    return ret;
                }
            }
            
            // 启动WiFi
            ret = esp_wifi_start();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "启动WiFi失败: %s", esp_err_to_name(ret));
                return ret;
            }
            
            // 保存WiFi开启状态和当前模式
            if (menu_nvs_manager) {
                menu_nvs_manager_save_wifi_state(menu_nvs_manager, true);
                
                // 获取并保存当前WiFi模式
                if (esp_wifi_get_mode(&current_mode) == ESP_OK) {
                    saved_wifi_mode = current_mode;
                    menu_nvs_manager_save_wifi_mode(menu_nvs_manager, saved_wifi_mode);
                }
            }
            
            ESP_LOGI(TAG, "WiFi已成功启动");
            return ESP_OK;
        }
        
        // WiFi已经在运行
        ESP_LOGI(TAG, "WiFi已经在运行，模式: %d", current_mode);
        
        // 确保模式被保存
        saved_wifi_mode = current_mode;
        
        return ESP_OK;
    } else {
        // 释放WiFi资源
        ESP_LOGI(TAG, "禁用WiFi，释放资源");
        
        // 保存WiFi关闭状态
        if (menu_nvs_manager) {
            menu_nvs_manager_save_wifi_state(menu_nvs_manager, false);
        }
        
        return wifi_release_resources();
    }
}

/**
 * @brief 清除保存的WiFi密码
 * @return ESP_OK表示成功
 */
esp_err_t wifi_clear_password(void)
{
    ESP_LOGI(TAG, "清除保存的WiFi密码");
    
    // 检查WiFi NVS管理器是否初始化，如果未初始化则尝试初始化
    if (!wifi_nvs_manager) {
        ESP_LOGI(TAG, "WiFi NVS管理器未初始化，尝试初始化");
        
        // 创建WiFi NVS管理器
        wifi_nvs_manager = wifi_nvs_manager_create();
        if (wifi_nvs_manager) {
            // 初始化WiFi NVS管理器
            esp_err_t ret = wifi_nvs_manager_init(wifi_nvs_manager);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "初始化WiFi NVS管理器失败: %s", esp_err_to_name(ret));
                return ret;
            }
        } else {
            ESP_LOGE(TAG, "创建WiFi NVS管理器失败");
            return ESP_FAIL;
        }
    }
    
    // 清除NVS中的WiFi配置（包括SSID和密码）
    esp_err_t err = wifi_nvs_manager_clear_config(wifi_nvs_manager);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "清除WiFi配置失败: %s", esp_err_to_name(err));
        return err;
    }
    
    // 获取当前WiFi模式
    wifi_mode_t current_mode;
    err = esp_wifi_get_mode(&current_mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "获取WiFi模式失败: %s", esp_err_to_name(err));
        return err;
    }
    
    // 设置新的WiFi模式为APSTA
    saved_wifi_mode = WIFI_MODE_APSTA;
    
    // 检查菜单NVS管理器是否初始化，如果未初始化则尝试初始化
    if (!menu_nvs_manager) {
        ESP_LOGI(TAG, "菜单NVS管理器未初始化，尝试初始化");
        
        // 创建菜单NVS管理器
        menu_nvs_manager = menu_nvs_manager_create(NULL, 0, false);
        if (menu_nvs_manager) {
            // 初始化菜单NVS管理器
            esp_err_t ret = menu_nvs_manager_init(menu_nvs_manager);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "初始化菜单NVS管理器失败: %s", esp_err_to_name(ret));
                // 继续执行，因为即使保存失败，清除密码的主要功能已经完成
            }
        } else {
            ESP_LOGE(TAG, "创建菜单NVS管理器失败");
            // 继续执行，因为即使保存失败，清除密码的主要功能已经完成
        }
    }
    
    // 保存新的WiFi模式到NVS
    if (menu_nvs_manager) {
        menu_nvs_manager_save_wifi_mode(menu_nvs_manager, saved_wifi_mode);
        ESP_LOGI(TAG, "已保存WiFi模式为APSTA");
    }
    
    // 检查WiFi是否打开
    if (current_mode != WIFI_MODE_NULL) {
        // WiFi当前是打开的，需要切换到APSTA模式
        ESP_LOGI(TAG, "WiFi当前打开，切换到APSTA模式");
        
        // 先关闭WiFi
        err = wifi_release_resources();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "释放WiFi资源失败: %s", esp_err_to_name(err));
            return err;
        }
        
        // 确保任务句柄为NULL，以便wifi_toggle创建新任务时使用已设置的APSTA模式
        wifi_task_handle = NULL;
        
        // 重新启动WiFi并使用APSTA模式
        err = wifi_toggle(true);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "重新启动WiFi失败: %s", esp_err_to_name(err));
            return err;
        }
    } else {
        // WiFi当前关闭，下次打开时会自动使用保存的APSTA模式
        ESP_LOGI(TAG, "WiFi当前关闭，下次打开时将自动进入APSTA模式");
    }
    
    return ESP_OK;
}

/**
 * @brief 获取HTTP服务器端口号
 * @return HTTP服务器端口号
 */
uint16_t wifi_get_http_port(void)
{
    return HTTP_SERVER_PORT;
}

/**
 * @brief 获取当前WiFi模式
 * @param mode 用于存储WiFi模式的指针
 * @return ESP_OK表示成功
 */
esp_err_t wifi_get_mode(wifi_mode_t *mode)
{
    if (!mode) {
        return ESP_FAIL;
    }
    return esp_wifi_get_mode(mode);
}

/**
 * @brief 获取WiFi连接状态
 * @return true表示已连接，false表示未连接
 */
bool wifi_is_connected(void)
{
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        return false;
    }
    
    if (mode & WIFI_MODE_STA) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief 获取AP模式下的SSID和密码
 * @param ssid SSID缓冲区
 * @param ssid_len SSID缓冲区大小
 * @param password 密码缓冲区
 * @param password_len 密码缓冲区大小
 * @return ESP_OK表示成功
 */
esp_err_t wifi_get_ap_info(char *ssid, size_t ssid_len, char *password, size_t password_len)
{
    if (!ssid || !password) {
        return ESP_FAIL;
    }
    
    wifi_config_t wifi_config;
    esp_err_t ret = esp_wifi_get_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        return ret;
    }
    
    strncpy(ssid, (char *)wifi_config.ap.ssid, ssid_len - 1);
    ssid[ssid_len - 1] = '\0';
    
    strncpy(password, (char *)wifi_config.ap.password, password_len - 1);
    password[password_len - 1] = '\0';
    
    return ESP_OK;
}

/**
 * @brief WiFi任务启动函数
 * 仅当WiFi任务未创建时创建并启动WiFi管理任务
 */
void wifi_task(void)
{
    if (wifi_task_handle == NULL) {
        xTaskCreate(app_wifi_task, "wifi_task", 4 * 1024, NULL, 4, &wifi_task_handle);
    }
}


