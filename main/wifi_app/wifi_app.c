#include "wifi_app.h"
#include "esp_err.h"
#include <time.h>
#include <string.h>
#include "esp_interface.h"
#include "esp_netif.h"
#include "esp_http_server.h"

// 日志标签
#define TAG "wifi_app_new"

// 配置常量
#define SCAN_LIST_SIZE 20          // 扫描列表大小
#define AP_SSID "ESP32-AP-Device"  // 默认AP名称
#define AP_PASSWORD "123456789"     // 默认AP密码
#define HTTP_SERVER_PORT 80        // HTTP服务器端口


wifi_state_t wifi_state = {
    .server = NULL,
    .sta_netif = NULL,
    .ap_netif = NULL,
    .client_ip = {0},
    .unified_nvs_manager = NULL,
    .wifi_task_handle = NULL,
    .wifi_event_handler_instance = NULL,
    .ip_event_handler_instance = NULL,
    .mode = WIFI_MODE_APSTA,
    .wifi_enable_state = false,
    .auto_shutdown_timer = 0,
};

static esp_err_t read_wifi_config(char *ssid, char *password);
static esp_err_t wifi_release_resources(void);


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
    if (wifi_state.unified_nvs_manager) {
        esp_err_t nvs_ret = unified_nvs_save_wifi_config(wifi_state.unified_nvs_manager, ssid, password);
        if (nvs_ret != ESP_OK) {
            ESP_LOGE(TAG, "保存WiFi配置失败: %s", esp_err_to_name(nvs_ret));
        } else {
            ESP_LOGI(TAG, "WiFi配置保存成功 - SSID: %s", ssid);
        }
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
            netif = wifi_state.sta_netif;
            ESP_LOGI(TAG, "Using connected STA interface");
        } else if (current_mode & WIFI_MODE_AP) {
            netif = wifi_state.ap_netif;
            ESP_LOGI(TAG, "Using AP interface (STA not connected)");
        }
    } else if (current_mode & WIFI_MODE_AP) {
        netif = wifi_state.ap_netif;
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
    if (netif == wifi_state.ap_netif) {
        // AP模式下返回已连接客户端IP
        if (strlen(wifi_state.client_ip) == 0) {
            const char *resp = "{\"status\":\"error\",\"message\":\"No client connected to AP\"}";
            httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        snprintf(ip, sizeof(ip), "%s", wifi_state.client_ip);
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
    
    // 解析层参数
    char query[50];
    size_t query_len = httpd_req_get_url_query_len(req) + 1;
    if (query_len > 1) {
        httpd_req_get_url_query_str(req, query, query_len);
        
        char layer_param[10];
        if (httpd_query_key_value(query, "layer", layer_param, sizeof(layer_param)) == ESP_OK) {
            int layer = atoi(layer_param);
            if (layer >= 1 && layer <= 6) {
                // 加载指定层的自定义映射
                uint16_t keymap[NUM_KEYS] = {0};
                esp_err_t err = load_keymap_from_nvs(layer, keymap);
                
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to load keymap layer %d: %s", layer, esp_err_to_name(err));
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
        }
    }
    
    // 如果没有指定层参数或参数无效，默认加载层1
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
    
    // 添加调试信息
    ESP_LOGI(TAG, "接收到的JSON数据: %s", buf);
    
    char *keymap_start = strstr(buf, "\"keymap\":[");
    
    if (keymap_start) {
        keymap_start += strlen("\"keymap\":[");
        char *keymap_end = strstr(keymap_start, "]");
        
        if (keymap_end) {
            // 计算数组长度
            size_t array_length = keymap_end - keymap_start;
            char *array_copy = malloc(array_length + 1);
            if (array_copy) {
                strncpy(array_copy, keymap_start, array_length);
                array_copy[array_length] = '\0';
                
                // 解析数组元素
                char *token = strtok(array_copy, ",");
                int index = 0;
                
                while (token != NULL && index < NUM_KEYS) {
                    // 去除可能的空格
                    while (*token == ' ') token++;
                    
                    keymap[index] = (uint16_t)strtoul(token, NULL, 10);
                    ESP_LOGI(TAG, "解析键码[%d]: %u", index, keymap[index]);
                    
                    token = strtok(NULL, ",");
                    index++;
                }
                
                free(array_copy);
                ESP_LOGI(TAG, "成功解析 %d 个键码", index);
                
                // 如果解析的键码数量少于NUM_KEYS，确保剩余位置保持为0
                if (index < NUM_KEYS) {
                    ESP_LOGW(TAG, "警告：只解析了 %d 个键码，期望 %d 个，剩余位置将保持为0", index, NUM_KEYS);
                }
            }
        } else {
            ESP_LOGE(TAG, "JSON解析错误: 未找到数组结束符]");
        }
    } else {
        ESP_LOGE(TAG, "JSON解析错误: 未找到keymap字段");
    }
    
    // 解析层参数
    int layer = 1; // 默认层1
    char *layer_start = strstr(buf, "\"layer\":");
    if (layer_start) {
        layer_start += strlen("\"layer\":");
        layer = (int)strtoul(layer_start, NULL, 10);
        if (layer < 1 || layer > 6) {
            layer = 1; // 如果层参数无效，使用默认层1
        }
    }
    
    ESP_LOGI(TAG, "保存按键映射到层 %d", layer);
    
    // 保存自定义映射到指定层
    esp_err_t err = save_keymap_to_nvs(layer, keymap);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save keymap to layer %d: %s", layer, esp_err_to_name(err));
        const char* resp = "{\"status\":\"error\",\"message\":\"保存按键映射失败\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    const char* resp = "{\"status\":\"success\",\"message\":\"按键映射保存成功\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief 保存单个按键接口处理函数
 * 保存用户配置的单个按键
 */
static esp_err_t save_single_key_handler(httpd_req_t *req)
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
    uint8_t key_index = 0;
    uint16_t key_code = 0;
    
    // 添加调试信息
    ESP_LOGI(TAG, "接收到的单个按键JSON数据: %s", buf);
    
    // 查找keyIndex字段
    char *index_start = strstr(buf, "\"keyIndex\":");
    if (index_start) {
        index_start += strlen("\"keyIndex\":");
        key_index = (uint8_t)strtoul(index_start, NULL, 10);
    }
    
    // 查找keyCode字段
    char *code_start = strstr(buf, "\"keyCode\":");
    if (code_start) {
        code_start += strlen("\"keyCode\":");
        key_code = (uint16_t)strtoul(code_start, NULL, 10);
    }
    
    // 验证参数
    if (key_index >= NUM_KEYS) {
        ESP_LOGE(TAG, "无效的按键索引: %d (最大: %d)", key_index, NUM_KEYS - 1);
        const char* resp = "{\"status\":\"error\",\"message\":\"无效的按键索引\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    // 解析层参数
    int layer = 1; // 默认层1
    char *layer_start = strstr(buf, "\"layer\":");
    if (layer_start) {
        layer_start += strlen("\"layer\":");
        layer = (int)strtoul(layer_start, NULL, 10);
        if (layer < 1 || layer > 6) {
            layer = 1; // 如果层参数无效，使用默认层1
        }
    }
    
    ESP_LOGI(TAG, "保存单个按键 - 层: %d, 索引: %d, 键码: %u", layer, key_index, key_code);
    
    // 保存单个按键到指定层
    esp_err_t err = save_single_key_to_nvs(layer, key_index, key_code);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save single key to layer %d: %s", layer, esp_err_to_name(err));
        const char* resp = "{\"status\":\"error\",\"message\":\"保存单个按键失败\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    const char* resp = "{\"status\":\"success\",\"message\":\"单个按键保存成功\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief 获取按键数量接口处理函数
 * 返回设备配置的按键数量
 */
static esp_err_t get_num_keys_handler(httpd_req_t *req)
{

    // 检查NUM_KEYS常量是否可用
    #ifndef NUM_KEYS
        ESP_LOGE(TAG, "NUM_KEYS常量未定义，使用默认值17");
        #define NUM_KEYS 17
    #endif
    
    // 验证NUM_KEYS的值
    if (NUM_KEYS <= 0 || NUM_KEYS > 255) {
        ESP_LOGE(TAG, "NUM_KEYS值无效: %d，使用默认值17", NUM_KEYS);
        #undef NUM_KEYS
        #define NUM_KEYS 17
    }
    
    char resp[100];
    int resp_len = snprintf(resp, sizeof(resp), "{\"status\":\"success\",\"numKeys\":%d}", NUM_KEYS);
    
    if (resp_len < 0 || resp_len >= sizeof(resp)) {
        ESP_LOGE(TAG, "响应缓冲区溢出，使用默认响应");
        const char* default_resp = "{\"status\":\"success\",\"numKeys\":17}";
        httpd_resp_send(req, default_resp, HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    }
    
    ESP_LOGI(TAG, "成功返回按键数量: %d", NUM_KEYS);
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

static const httpd_uri_t save_single_key_uri = {
    .uri       = "/save-single-key",
    .method    = HTTP_POST,
    .handler   = save_single_key_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t get_num_keys_uri = {
    .uri       = "/get-num-keys",
    .method    = HTTP_GET,
    .handler   = get_num_keys_handler,
    .user_ctx  = NULL
};

/**
 * @brief 启动HTTP服务器并注册URI处理程序
 * @return ESP_OK表示服务器启动成功
 */
static esp_err_t start_webserver(void)
{
    // 如果服务器已在运行，先停止它
    if (wifi_state.server != NULL) {
        httpd_stop(wifi_state.server);
        wifi_state.server = NULL;
        ESP_LOGI(TAG, "已停止现有HTTP服务器实例");
    }

    // 定义HTTP服务器配置
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.recv_wait_timeout = 5; // 增加接收超时时间(秒)
    config.send_wait_timeout = 5; // 增加发送超时时间(秒)
    config.lru_purge_enable = true; // 启用LRU缓存清理
    config.max_uri_handlers = 16; // 默认是8，增加到16
    
    esp_err_t start_ret = httpd_start(&wifi_state.server, &config);

    if (start_ret == ESP_OK) {
        // 注册URI处理程序
        httpd_register_uri_handler(wifi_state.server, &index_uri);
        httpd_register_uri_handler(wifi_state.server, &favicon_uri);
        httpd_register_uri_handler(wifi_state.server, &connect_wifi_uri);
        httpd_register_uri_handler(wifi_state.server, &scan_wifi_uri);
        httpd_register_uri_handler(wifi_state.server, &get_ip_uri);
        httpd_register_uri_handler(wifi_state.server, &load_keymap_uri);
        httpd_register_uri_handler(wifi_state.server, &save_keymap_uri);
        httpd_register_uri_handler(wifi_state.server, &save_single_key_uri);
        httpd_register_uri_handler(wifi_state.server, &get_num_keys_uri);
        ESP_LOGI(TAG, "HTTP服务器启动成功");
    } else {
        ESP_LOGE(TAG, "HTTP服务器启动失败: %s", esp_err_to_name(start_ret));
        wifi_state.server = NULL;
    }

    return start_ret;
}

/**
 * @brief 停止HTTP服务器
 */
static void stop_webserver(void)
{
    if (wifi_state.server != NULL) {
        httpd_stop(wifi_state.server);
        wifi_state.server = NULL;
        ESP_LOGI(TAG, "HTTP服务器已停止");
    }
}


// 全局变量用于跟踪连接状态
static bool g_sta_connected = false;
static uint8_t g_ap_client_count = 0;

/**
 * @brief 检查APSTA模式下是否有设备连接
 * @return true表示有设备连接，false表示无设备连接
 */
static bool check_apsta_connections(void)
{
    wifi_mode_t current_mode;
    if (esp_wifi_get_mode(&current_mode) != ESP_OK) {
        return true; // 如果获取模式失败，保守起见认为有连接
    }
    
    // 只有在APSTA模式下才进行连接检测
    if (current_mode != WIFI_MODE_APSTA) {
        return true; // 非APSTA模式，不进行自动关闭
    }
    
    // 使用事件驱动的状态跟踪，避免频繁轮询
    return g_sta_connected || (g_ap_client_count > 0);
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
            case WIFI_EVENT_AP_STADISCONNECTED: {
                memset(wifi_state.client_ip, 0, sizeof(wifi_state.client_ip));
                
                // 更新AP客户端连接状态
                if (g_ap_client_count > 0) {
                    g_ap_client_count--;
                }
                
                // 当AP客户端断开连接时，重置自动关闭计时器
                wifi_state.auto_shutdown_timer = 0;
                ESP_LOGI(TAG, "AP客户端断开连接，当前客户端数: %d，重置自动关闭计时器", g_ap_client_count);
                
                break;
            }
            case WIFI_EVENT_AP_STACONNECTED: {
                // 更新AP客户端连接状态
                g_ap_client_count++;
                
                // 当AP有客户端连接时，重置自动关闭计时器
                wifi_state.auto_shutdown_timer = 0;
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "AP客户端连接，MAC: %02x:%02x:%02x:%02x:%02x:%02x，当前客户端数: %d，重置自动关闭计时器",
                        event->mac[0], event->mac[1], event->mac[2],
                        event->mac[3], event->mac[4], event->mac[5], g_ap_client_count);
                break;
            }
            case WIFI_EVENT_STA_DISCONNECTED: {
                // 更新STA连接状态
                g_sta_connected = false;
                
                // 当STA断开连接时，重置自动关闭计时器
                wifi_state.auto_shutdown_timer = 0;
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGI(TAG, "STA断开连接，原因: %d，重置自动关闭计时器", event->reason);
                break;
            }
            case WIFI_EVENT_STA_CONNECTED: {
                // 更新STA连接状态
                g_sta_connected = true;
                
                // 当STA连接成功时，重置自动关闭计时器
                wifi_state.auto_shutdown_timer = 0;
                ESP_LOGI(TAG, "STA连接成功，重置自动关闭计时器");
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

                strncpy(wifi_state.client_ip, ip_str, sizeof(wifi_state.client_ip) - 1);
                ESP_LOGI(TAG, "Web服务器地址已保存: %s", wifi_state.client_ip);
                
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
                
                // 保存STA模式到NVS
                if (wifi_state.unified_nvs_manager) { 
                    esp_err_t ret = unified_nvs_save_wifi_state_config(wifi_state.unified_nvs_manager, 
                                                                      WIFI_MODE_STA);
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "STA模式已保存到NVS");
                    } else {
                        ESP_LOGE(TAG, "保存STA模式到NVS失败: %s", esp_err_to_name(ret));
                    }
                }
                
                break;
            }
            case IP_EVENT_AP_STAIPASSIGNED: {
                // 保存AP设备本身的IP地址192.168.4.1
                snprintf(wifi_state.client_ip, sizeof(wifi_state.client_ip), "192.168.4.1");
                ESP_LOGI(TAG, "AP IP地址: %s", wifi_state.client_ip);
                break;
            }
            default:
                ESP_LOGI(TAG, "其他IP_EVENT: %" PRIu32, event_id);
                break;
        }
    }
}

esp_err_t wifi_station_change(bool enable)
{   
    esp_err_t ret = ESP_OK;
    
    // 先保存WiFi启用状态到NVS（在WiFi任务启动/关闭之前）
    // 使用全局NVS管理器，确保在WiFi任务初始化之前也能保存状态
    extern unified_nvs_manager_t *g_unified_nvs_manager;
    
    if (g_unified_nvs_manager) {
        ret = UNIFIED_NVS_SAVE_BOOL(g_unified_nvs_manager, NVS_NAMESPACE_WIFI, "enabled", enable);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "WiFi启用状态已保存到NVS: %s", enable ? "启用" : "禁用");
            wifi_state.wifi_enable_state = enable ;
        } else {
            ESP_LOGE(TAG, "保存WiFi启用状态失败: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "统一NVS管理器未初始化，无法保存WiFi启用状态");
        ret = ESP_FAIL;
    }
    
    if (enable) {
        // 启用WiFi - 创建WiFi任务
        if (wifi_state.wifi_task_handle == NULL) {
            ESP_LOGI(TAG, "启用WiFi，创建WiFi任务");
            wifi_task();
        } else {
            ESP_LOGI(TAG, "WiFi任务已存在，无需重新创建");
        }
    } else {
        // 禁用WiFi - 删除WiFi任务并清理资源
        if (wifi_state.wifi_task_handle != NULL) {
            ESP_LOGI(TAG, "禁用WiFi，删除WiFi任务");
            
            // 使用专门的资源释放函数
            esp_err_t release_ret = wifi_release_resources();
            if (release_ret != ESP_OK) {
                ESP_LOGE(TAG, "释放WiFi资源失败: %s", esp_err_to_name(release_ret));
            }
            
            // 删除WiFi任务
            vTaskDelete(wifi_state.wifi_task_handle);
            wifi_state.wifi_task_handle = NULL;
            
            ESP_LOGI(TAG, "WiFi任务已删除，资源已清理");
        } else {
            ESP_LOGI(TAG, "WiFi任务不存在，无需删除");
        }
    }
    
    return ret;
}

/**
 * @brief 释放WiFi相关资源
 * 清理WiFi任务、网络接口、事件处理程序等资源
 */
static esp_err_t wifi_release_resources(void)
{
    esp_err_t ret = ESP_OK;
    
    // 停止HTTP服务器
    stop_webserver();
    
    // 停止WiFi
    esp_wifi_stop();
    
    // 注销事件处理程序
    if (wifi_state.wifi_event_handler_instance) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_state.wifi_event_handler_instance);
        wifi_state.wifi_event_handler_instance = NULL;
    }
    if (wifi_state.ip_event_handler_instance) {
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, wifi_state.ip_event_handler_instance);
        wifi_state.ip_event_handler_instance = NULL;
    }
    
    // 清理网络接口
    if (wifi_state.sta_netif) {
        esp_netif_destroy(wifi_state.sta_netif);
        wifi_state.sta_netif = NULL;
    }
    if (wifi_state.ap_netif) {
        esp_netif_destroy(wifi_state.ap_netif);
        wifi_state.ap_netif = NULL;
    }
    
    // 清理WiFi任务句柄
    if (wifi_state.wifi_task_handle) {
        wifi_state.wifi_task_handle = NULL;
    }
    
    // 重置客户端IP
    memset(wifi_state.client_ip, 0, sizeof(wifi_state.client_ip));
    
    ESP_LOGI(TAG, "WiFi资源释放完成");
    return ret;
}

/**
 * @brief 清除保存的WiFi密码配置
 * 清除NVS中保存的WiFi连接信息，并将WiFi模式设置为APSTA模式
 */
esp_err_t wifi_clear_password(void)
{
    esp_err_t ret = ESP_OK;
    
    // 使用全局NVS管理器，确保在WiFi关闭状态下也能清理密码
    extern unified_nvs_manager_t *g_unified_nvs_manager;
    unified_nvs_manager_t *nvs_manager = NULL;
    
    // 优先使用WiFi状态中的NVS管理器，如果不存在则使用全局NVS管理器
    if (wifi_state.unified_nvs_manager) {
        nvs_manager = wifi_state.unified_nvs_manager;
        ESP_LOGI(TAG, "使用WiFi状态中的NVS管理器清理密码");
    } else if (g_unified_nvs_manager) {
        nvs_manager = g_unified_nvs_manager;
        ESP_LOGI(TAG, "使用全局NVS管理器清理密码");
    } else {
        ESP_LOGE(TAG, "统一NVS管理器未初始化，无法清理密码");
        return ESP_FAIL;
    }
    
    // 设置WiFi模式为APSTA模式
    wifi_state.mode = WIFI_MODE_APSTA;
    
    // 保存模式配置到NVS
    ret = unified_nvs_save_wifi_state_config(nvs_manager, wifi_state.mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存WiFi模式失败: %s", esp_err_to_name(ret));
    }
    
    // 清除WiFi密码
    esp_err_t clear_ret = unified_nvs_clear_wifi_password(nvs_manager);
    if (clear_ret != ESP_OK) {
        ESP_LOGE(TAG, "清除WiFi密码失败: %s", esp_err_to_name(clear_ret));
    } else {
        ESP_LOGI(TAG, "WiFi密码已清除");
    }
    
    // 在启动WiFi的状态下清除密码并重启WiFi任务
    if(wifi_state.wifi_enable_state == 1) {

        // 重启WiFi任务
        if (wifi_state.wifi_task_handle != NULL) {
            // 首先停止WiFi连接
            esp_wifi_disconnect();
            
            // 重置WiFi配置，清除缓存
            wifi_config_t empty_config = {0};
            esp_wifi_set_config(WIFI_IF_STA, &empty_config);
            
            // 释放WiFi资源
            esp_err_t release_ret = wifi_release_resources();
            if (release_ret != ESP_OK) {
                ESP_LOGE(TAG, "释放WiFi资源失败: %s", esp_err_to_name(release_ret));
            }
            
            // 删除WiFi任务
            vTaskDelete(wifi_state.wifi_task_handle);
            wifi_state.wifi_task_handle = NULL;
            
            ESP_LOGI(TAG, "WiFi任务已删除，准备重新启动");
        }
        
        // 重新创建WiFi任务
        wifi_task();
        ESP_LOGI(TAG, "WiFi任务已重新启动");
    }

    return ESP_OK;
}

/**
 * @brief 启用热点模式
 * 设置STA为默认接口并启用NAPT功能
 */
esp_err_t wifi_hotspot(void)
{
    esp_err_t ret = ESP_OK;
    
    // 设置STA为默认接口
    if (wifi_state.sta_netif) {
        esp_netif_set_default_netif(wifi_state.sta_netif);
    }

    // 在AP网络接口上启用NAPT（网络地址端口转换）
    if (wifi_state.ap_netif) {
        ret = esp_netif_napt_enable(wifi_state.ap_netif);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "在netif上启用NAPT失败: %p", wifi_state.ap_netif);
            return ret;
        }
        ESP_LOGI(TAG, "AP网络接口NAPT已启用");
    } else {
        ESP_LOGW(TAG, "AP网络接口未初始化，无法启用NAPT");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_netif_t* wifi_init_softap(void){

    esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

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

    if (strlen(AP_PASSWORD) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));


    return esp_netif_ap;
}

esp_netif_t* wifi_init_sta(){

    char ssid[32];
    char password[64];

    esp_err_t ret = ESP_OK;

    ret = read_wifi_config(ssid,password);

    esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    // 只有当读取配置成功时才设置STA配置
    if(ret == ESP_OK){

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

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config) );

    } else {
        wifi_config_t empty_config = {0};
        esp_wifi_set_config(WIFI_IF_STA, &empty_config);
    }
    
    return esp_netif_sta;
}


/**
 * @brief 检查WiFi连接状态
 * @return true表示已连接，false表示未连接
 */
bool wifi_is_connected(void)
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return true;
    }
    return false;
}

/**
 * @brief 获取当前WiFi模式
 * @param mode 用于存储WiFi模式的指针
 * @return ESP_OK表示成功
 */
esp_err_t wifi_get_mode(wifi_mode_t *mode)
{
    if (!mode) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = esp_wifi_get_mode(mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取WiFi模式失败: %s", esp_err_to_name(ret));
    }
    
    return ret;
}



/**
 * @brief WiFi任务主函数
 */
void wifi_init_task(void *pvParameters)
{
    ESP_LOGI(TAG, "WiFi任务启动");
    
    // 初始化NVS系统
    esp_err_t nvs_ret = wifi_app_nvs_init();
    if (nvs_ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi NVS初始化失败，任务退出");
        vTaskDelete(NULL);
        return;
    }
    
    // 加载WiFi配置
    uint8_t wifi_mode;
    esp_err_t ret = unified_nvs_load_wifi_state_config(wifi_state.unified_nvs_manager, &wifi_mode);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "加载WiFi模式失败: %s，使用默认模式APSTA", esp_err_to_name(ret));
        wifi_state.mode = WIFI_MODE_APSTA;
    } else {
        wifi_state.mode = (wifi_mode_t)wifi_mode;
    }
    ESP_LOGI(TAG, "根据保存的配置初始化WiFi - 模式: %d", wifi_state.mode);

    // 初始化基础网络和创建事件
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 注册事件处理程序
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &wifi_state.wifi_event_handler_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &wifi_state.ip_event_handler_instance));

    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // 设置WiFi模式
    if (wifi_state.mode == WIFI_MODE_STA) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    } else {
        // 默认使用APSTA模式
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    }
    
    // 根据WiFi模式初始化网络接口
    if (wifi_state.mode == WIFI_MODE_APSTA) {
        // APSTA模式下同时初始化AP和STA接口
        wifi_state.ap_netif = wifi_init_softap();
        wifi_state.sta_netif = wifi_init_sta();
    } else if (wifi_state.mode == WIFI_MODE_AP) {
        // AP模式下只初始化AP接口
        wifi_state.ap_netif = wifi_init_softap();
    } else if (wifi_state.mode == WIFI_MODE_STA) {
        // STA模式下只初始化STA接口
        wifi_state.sta_netif = wifi_init_sta();
    }
    
    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // 只有在STA模式下且有有效配置时才尝试连接
    if (wifi_state.mode == WIFI_MODE_STA || wifi_state.mode == WIFI_MODE_APSTA) {
        // 检查是否有有效的STA配置
        wifi_config_t current_config;
        if (esp_wifi_get_config(WIFI_IF_STA, &current_config) == ESP_OK) {
            // 只有当SSID不为空时才尝试连接
            if (current_config.sta.ssid[0] != '\0') {
                ESP_ERROR_CHECK(esp_wifi_connect());
            } 
        }
    }

    ESP_ERROR_CHECK(start_webserver());

    // 初始化自动关闭计时器
    wifi_state.auto_shutdown_timer = 0;
    
    // 主任务循环 - 保持WiFi功能运行
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        
        // 检查APSTA模式下的连接状态
        if (!check_apsta_connections()) {
            // 无设备连接，增加计时器
            wifi_state.auto_shutdown_timer++;
            
            // 每30秒记录一次状态
            if (wifi_state.auto_shutdown_timer % 30 == 0) {
                ESP_LOGI(TAG, "APSTA模式无设备连接，计时器: %" PRIu32 "秒", wifi_state.auto_shutdown_timer);
            }
            
            // 如果连续5分钟无设备连接，自动关闭WiFi
            if (wifi_state.auto_shutdown_timer >= 300) { // 5分钟 = 300秒
                ESP_LOGI(TAG, "APSTA模式连续5分钟无设备连接，自动关闭WiFi");              
                // 使用wifi_station_change函数关闭WiFi，它会自动保存状态到NVS并清理资源
                wifi_station_change(false);
                
            }
        } else {
            // 有设备连接，重置计时器
            wifi_state.auto_shutdown_timer = 0;
        }
    }
    
    vTaskDelete(NULL);
}

/**
 * @brief WiFi任务启动函数
 * 仅当WiFi任务未创建时创建并启动WiFi管理任务
 */
void wifi_task(void)
{
    if (wifi_state.wifi_task_handle == NULL) {
        xTaskCreate(wifi_init_task, "wifi_init_task", 4 * 1024, NULL, 4, &wifi_state.wifi_task_handle);
    }
}

/**
 * @brief 初始化WiFi应用的NVS系统
 * 使用全局的统一NVS管理器，确保NVS权限管理一致性
 * @return ESP_OK表示初始化成功
 */
esp_err_t wifi_app_nvs_init(void)
{

    // 使用全局的统一NVS管理器，确保与初始化管理器保持一致
    extern unified_nvs_manager_t *g_unified_nvs_manager;
    
    if (!g_unified_nvs_manager) {
        ESP_LOGE(TAG, "全局统一NVS管理器未初始化");
        return ESP_FAIL;
    }
    
    // 设置WiFi模块使用全局NVS管理器
    wifi_state.unified_nvs_manager = g_unified_nvs_manager;
    
    ESP_LOGI(TAG, "WiFi NVS系统初始化成功，使用全局NVS管理器");
    return ESP_OK;
}

/**
 * @brief 从NVS存储中读取WiFi配置信息
 * @param ssid 用于存储读取到的SSID的缓冲区
 * @param password 用于存储读取到的密码的缓冲区
 * @return ESP_OK表示读取成功
 */
static esp_err_t read_wifi_config(char *ssid, char *password)
{
    // 检查统一NVS管理器是否已初始化
    if (!wifi_state.unified_nvs_manager) {
        ESP_LOGE(TAG, "统一NVS管理器未初始化");
        return ESP_FAIL;
    }

    // 使用专用的WiFi配置读取函数
    esp_err_t ret = unified_nvs_load_wifi_config(wifi_state.unified_nvs_manager, 
                                                ssid, 32, 
                                                password, 64);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "读取WiFi配置成功 - SSID: %s", ssid);
    } else {
        ESP_LOGW(TAG, "读取WiFi配置失败: %s", esp_err_to_name(ret));
    }
    
    return ret;
}