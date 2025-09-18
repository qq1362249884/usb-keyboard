#include "wifi_app.h"
#include <assert.h>
#include "freertos/task.h"
#include "nvs_manager/wifi_nvs_manager.h"
#include "spi_scanner/keymap_manager.h"


#define SCAN_LIST_SIZE 20 // 扫描列表大小
esp_netif_t *sta_netif = NULL; // 全局STA网络接口句柄
esp_netif_t *ap_netif = NULL; // 声明AP网络接口
char client_ip[16] = {0}; // 存储客户端IP地址，extern声明在wifi_app.h中
httpd_handle_t server = NULL; // 用于存储httpd的句柄


static const char *TAG1 = "wifi AP";
static const char *TAG2 = "wifi STA";
static const char *TAG3 = "wifi STA_AP";
static const char *TAG4 = "wifi nvs";
const int SCAN_DONE_BIT = BIT0;

// WiFi NVS管理器实例
static WifiNvsManager_t* wifi_nvs_manager = NULL;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");




/**
 * @brief 处理根路径(/)的HTTP请求，返回HTML页面
 *
 * @param req HTTP请求对象
 * @return esp_err_t 返回ESP_OK表示处理成功
 */
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);

    return ESP_OK;
}

static esp_err_t favicon_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, NULL, 0); // 返回空图标
    return ESP_OK;
}

/**
 * @brief 连接WiFi接口处理函数
 * 处理POST请求，解析SSID和密码并尝试连接WiFi
 */
static esp_err_t connect_wifi_handler(httpd_req_t *req) {
    char buf[100];
    ssize_t len = httpd_req_recv(req, buf, sizeof(buf)-1);
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
            strncpy(ssid, ssid_start, (ssid_end - ssid_start) < (sizeof(ssid)-1) ? (ssid_end - ssid_start) : (sizeof(ssid)-1));
        }
    }
    
    // 查找password字段
    char *password_start = strstr(buf, "\"password\":\"");
    char *password_end = NULL;
    if (password_start) {
        password_start += strlen("\"password\":\"");
        password_end = strstr(password_start, "\"");
        if (password_end) {
            strncpy(password, password_start, (password_end - password_start) < (sizeof(password)-1) ? (password_end - password_start) : (sizeof(password)-1));
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
            ESP_LOGE(TAG4, "保存WiFi配置失败: %s", esp_err_to_name(nvs_ret));
        } else {
            ESP_LOGI(TAG4, "WiFi配置保存成功 - SSID: %s", ssid);
        }
    } else {
        ESP_LOGE(TAG4, "WiFi NVS管理器未初始化，无法保存配置");
    }
    
    // 配置并连接WiFi
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid)-1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password)-1);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_err_t ret = esp_wifi_connect();
    
    if (ret == ESP_OK) {
        const char* resp = "{\"status\":\"success\",\"message\":\"连接请求已发送，正在尝试连接...\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    } else {
        const char* resp = "{\"status\":\"error\",\"message\":\"连接请求失败: %s\"}";
        char error_resp[100];
        snprintf(error_resp, sizeof(error_resp), resp, esp_err_to_name(ret));
        httpd_resp_send(req, error_resp, HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

/**
 * @brief 扫描WiFi列表接口处理函数
 * 执行WiFi扫描并返回可用SSID列表
 */
static esp_err_t scan_wifi_handler(httpd_req_t *req) {

    //定义局部变量
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
        ESP_LOGE(TAG2, "扫描WiFi失败: %s", esp_err_to_name(scan_ret));
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
    
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_records) {
        ESP_LOGE(TAG2, "内存分配失败");
        const char* resp = "{\"status\":\"error\",\"message\":\"内存分配失败\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));
    // 去重并保留信号最强的WiFi
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
            strncpy(unique_aps[unique_count].ssid, (char*)ap_records[i].ssid, sizeof(unique_aps[unique_count].ssid)-1);
            unique_aps[unique_count].rssi = ap_records[i].rssi;
            unique_count++;
        }
    }

    // 按信号强度排序（从强到弱）
    for (int i = 0; i < unique_count; i++) {
        for (int j = i+1; j < unique_count; j++) {
            if (unique_aps[j].rssi > unique_aps[i].rssi) {
                UniqueAP temp = unique_aps[i];
                unique_aps[i] = unique_aps[j];
                unique_aps[j] = temp;
            }
        }
    }
    ESP_LOGI(TAG2, "扫描到的AP数量: %d (去重后: %d)", ap_count, unique_count);
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

static esp_err_t get_ip_handler(httpd_req_t *req) {
    // 获取IP地址
    esp_netif_ip_info_t ip_info;
    memset(&ip_info, 0, sizeof(ip_info));
    
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
    ESP_LOGI("get_ip_handler", "Current WiFi mode: %d", current_mode);
    
    // 对于APSTA模式，优先使用STA接口（如果已连接）
    if (current_mode & WIFI_MODE_STA) {
        // 检查STA是否已连接
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            netif = sta_netif;
            ESP_LOGI("get_ip_handler", "Using connected STA interface");
        } else if (current_mode & WIFI_MODE_AP) {
            netif = ap_netif;
            ESP_LOGI("get_ip_handler", "Using AP interface (STA not connected)");
        }
    } else if (current_mode & WIFI_MODE_AP) {
        netif = ap_netif;
        ESP_LOGI("get_ip_handler", "Using AP interface");
    }

    // 检查接口是否有效
    if (!netif) {
        const char *resp = "{\"status\":\"error\",\"message\":\"Network interface not available\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    if (netif == NULL) {
        const char *resp = "{\"status\":\"error\",\"message\":\"No active network interface\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    // 获取IP信息
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
static esp_err_t load_keymap_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    // 加载自定义映射（层1）
    uint16_t keymap[NUM_KEYS] = {0};
    esp_err_t err = load_keymap_from_nvs(1, keymap);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG2, "Failed to load keymap: %s", esp_err_to_name(err));
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
static esp_err_t save_keymap_handler(httpd_req_t *req) {
    char buf[512];
    ssize_t len = httpd_req_recv(req, buf, sizeof(buf)-1);
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
        ESP_LOGE(TAG2, "Failed to save keymap: %s", esp_err_to_name(err));
        const char* resp = "{\"status\":\"error\",\"message\":\"保存按键映射失败\"}";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    const char* resp = "{\"status\":\"success\",\"message\":\"按键映射保存成功\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief 定义index.html文件的URI和回调函数
 * 
 * 该函数用于定义index.html文件的URI和回调函数，当客户端请求该URI时，服务器会调用index_handler函数来处理请求。
 * 
 * @param server httpd句柄，用于注册URI
 * @param index_uri httpd_uri_t类型的结构体，用于存储URI的相关信息
 * @param index_handler 回调函数，用于处理URI请求
 * 
 * @return esp_err_t 返回ESP_OK表示成功，其他值表示失败
*/
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

// 连接WiFi接口URI配置
static const httpd_uri_t connect_wifi_uri = {
    .uri       = "/connect-wifi",
    .method    = HTTP_POST,
    .handler   = connect_wifi_handler,
    .user_ctx  = NULL
};

// 扫描WiFi列表接口URI配置
static const httpd_uri_t scan_wifi_uri = {
    .uri       = "/scan-wifi",
    .method    = HTTP_GET,
    .handler   = scan_wifi_handler,
    .user_ctx  = NULL
};

// 获取当前IP接口URI配置
static const httpd_uri_t get_ip_uri = {
    .uri       = "/get-ip",
    .method    = HTTP_GET,
    .handler   = get_ip_handler,
    .user_ctx  = NULL
};

// 加载按键映射接口URI配置
static const httpd_uri_t load_keymap_uri = {
    .uri       = "/load-keymap",
    .method    = HTTP_GET,
    .handler   = load_keymap_handler,
    .user_ctx  = NULL
};

// 保存按键映射接口URI配置
static const httpd_uri_t save_keymap_uri = {
    .uri       = "/save-keymap",
    .method    = HTTP_POST,
    .handler   = save_keymap_handler,
    .user_ctx  = NULL
};

/**
 * @brief 启动HTTP服务器并注册URI处理程序
 *
 * 初始化HTTP服务器配置并启动服务，成功启动后注册指定的URI处理程序。
 *
 * @return esp_err_t 返回ESP_OK表示服务器启动成功
 */
static esp_err_t start_webserver(void)
{
    // 如果服务器已在运行，先停止它
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG2, "已停止现有HTTP服务器实例");
    }

    // 定义HTTP服务器配置
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // 请求头最大长度已在sdkconfig中设置为8192 (CONFIG_HTTPD_MAX_REQ_HDR_LEN=8192)
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
        ESP_LOGI(TAG2, "HTTP服务器启动成功");
    } else {
        ESP_LOGE(TAG2, "HTTP服务器启动失败: %s", esp_err_to_name(start_ret));
        server = NULL;
    }

    return start_ret;
}



/**
 * @brief 从NVS存储中读取WiFi配置信息
 *
 * 该函数使用WiFi NVS管理器读取预先存储的WiFi SSID和密码。
 *
 * @param ssid     用于存储读取到的SSID的缓冲区
 * @param password 用于存储读取到的密码的缓冲区
 *
 * @return
 *     - ESP_OK: 读取成功
 *     - 其他: 读取失败的错误码
 *
 * @note 调用者需要确保ssid和password缓冲区足够大
 */
static esp_err_t read_wifi_config(char *ssid, char *password)
{
    // 检查WiFi NVS管理器是否已初始化
    if (!wifi_nvs_manager) {
        ESP_LOGE(TAG4, "WiFi NVS管理器未初始化");
        return ESP_FAIL;
    }

    // 检查配置是否存在
    if (!wifi_nvs_manager_has_config(wifi_nvs_manager)) {
        ESP_LOGW(TAG4, "WiFi配置不存在");
        return ESP_ERR_NVS_NOT_FOUND;
    }

    // 读取WiFi配置
    esp_err_t ret = wifi_nvs_manager_load_config(wifi_nvs_manager, 
                                               ssid, 
                                               32, // 假设SSID缓冲区至少32字节
                                               password, 
                                               64); // 假设密码缓冲区至少64字节
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG4, "Read WiFi config success - SSID: %s", ssid);
    } else {
        ESP_LOGW(TAG4, "Failed to read WiFi config: %s", esp_err_to_name(ret));
    }
    
    return ret;
}



/**
 * @brief WiFi和IP事件处理函数
 *
 * 该函数处理来自WiFi和IP层的事件，包括：
 * - WiFi连接状态变化（启动、连接、断开）
 * - WiFi扫描完成事件
 * - IP地址获取事件
 *
 * @param arg 用户自定义参数（未使用）
 * @param event_base 事件基类型（WIFI_EVENT/IP_EVENT）
 * @param event_id 具体事件ID
 * @param event_data 事件相关数据
 *
 * @note 对于WiFi断开事件，会打印断开原因代码
 */
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch(event_id)
        {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG2, "WIFI_EVENT_STA_START");
                //ESP_ERROR_CHECK(esp_wifi_connect());
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG2, "WIFI_EVENT_STA_CONNECTED");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG2, "WIFI_EVENT_STA_DISCONNECTED");
                // 获取断开原因并打印
                wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*)event_data;
                ESP_LOGI(TAG2, "Disconnected reason: %d", disconnected->reason);
                //ESP_ERROR_CHECK(esp_wifi_connect());
                break;
            case WIFI_EVENT_SCAN_DONE :
                ESP_LOGI(TAG2, "WIFI_EVENT_SCAN_DONE");
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG1, "station "MACSTR" join, AID=%d",MAC2STR(event->mac), event->aid);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG1, "station "MACSTR" leave, AID=%d, reason=%d",MAC2STR(event->mac), event->aid, event->reason);
                // 清除客户端IP地址，以便在没有客户端连接时显示STA的IP地址
                memset(client_ip, 0, sizeof(client_ip));
                ESP_LOGI(TAG1, "Client IP cleared");
                break;
            }
            default:
                ESP_LOGI(TAG2, "Other WIFI_EVENT: %" PRIu32, event_id);
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
                ESP_LOGI(TAG2, "Device connected to AP, got IP: %s", ip_str);
                
                strncpy(client_ip, ip_str, sizeof(client_ip)-1);
                ESP_LOGI(TAG2, "Web服务器地址已保存: %s", client_ip);
                
                // 关闭AP模式
                ESP_LOGI(TAG2, "AP mode disabled after STA connection");
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
                
                break;
            }
            case IP_EVENT_AP_STAIPASSIGNED: {
                ip_event_ap_staipassigned_t* event = (ip_event_ap_staipassigned_t*) event_data;
                snprintf(client_ip, sizeof(client_ip), IPSTR, IP2STR(&event->ip));
                ESP_LOGI("wifi_app", "Client IP assigned: %s", client_ip);
                break;
            }
            default:
                ESP_LOGI(TAG2, "Other IP_EVENT: %" PRIu32, event_id);
                break;
        }
    }
}

//暂时没有用到
esp_err_t wifi_ap_init()
{
    ESP_ERROR_CHECK(nvs_flash_init()); //保存WiFi的ssid和密码
    ESP_ERROR_CHECK(esp_netif_init()); //初始化tcp/ip协议网络
    ESP_ERROR_CHECK(esp_event_loop_create_default());//创建事件系统循环
    esp_netif_create_default_wifi_ap(); //创建WiFi AP
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();//使用默认配置
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); //初始化WiFi

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL,NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32-AP-Device",
            .ssid_len = strlen((const char*)wifi_config.ap.ssid),
            .channel = 1,
            .password = "123456789",
            .max_connection = 2,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                    .required = false,
            },
        },
    };
 
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP) ); //设置AP模式
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config) ); //设置AP配置
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG1, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
        wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);


    return ESP_OK;
}
//暂时没有用到
esp_err_t wifi_sta_init()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL); //注册wifi事件
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL); //注册IP事件

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            }
        },
    };

    wifi_scan_config_t scan_config = {
        .show_hidden = false,
    };
    esp_wifi_scan_start(&scan_config, true);

    uint16_t number = SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    bool found_state = false;

    ESP_LOGI(TAG2, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(TAG2, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    for (int i = 0; i < number; i++) 
    {
        ESP_LOGI(TAG2, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG2, "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(TAG2, "Channel \t\t%d", ap_info[i].primary);
        if(strcmp((char *)ap_info[i].ssid, "12号楼WIFI") == 0)
        {
            
            if(found_state == false)
            {
                const char *password_str = "qq1362249884";
                memcpy(&wifi_config.sta.ssid, ap_info[i].ssid, sizeof(ap_info[i].ssid));
                memcpy(&wifi_config.sta.password, password_str, strlen(password_str));
                ESP_LOGI(TAG2, "匹配到%s", ap_info[i].ssid);
                ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
                vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second
                ESP_ERROR_CHECK(esp_wifi_connect());
            }
            found_state = true;

        }

    }


    return ESP_OK;
}


/**
 * @brief WiFi任务主函数
 * 负责WiFi初始化和管理，包括AP+STA模式设置、连接管理和Web服务器
 */
static void app_wifi_task(void *pvParameters)
{
    //局部变量
    char sta_ssid[32];
    char sta_password[64];
    //WiFi初始化配置
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
            ESP_LOGE(TAG4, "Failed to initialize WiFi NVS manager: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG4, "Failed to create WiFi NVS manager");
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ap_netif  = esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL,NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL,NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    //先设置AP模式
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = "ESP32-AP-Device",
            .ssid_len = strlen((const char*)wifi_ap_config.ap.ssid),
            .channel = 1,
            .password = "123456789",
            .max_connection = 2,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(500));//等待初始化完成

    // 无论是否有保存的WiFi配置，都始终启动Web服务器
    ESP_LOGI(TAG3, "启动Web服务器");
    ESP_ERROR_CHECK(start_webserver());
    
    // 如果有保存的WiFi配置，则尝试连接
    if(read_wifi_config(sta_ssid, sta_password) == ESP_OK){
        ESP_LOGI(TAG3, "读取wifi_SSID: %s", sta_ssid);
        ESP_LOGI(TAG3, "读取wifi_PASSWORD: %s", sta_password);
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
        ESP_ERROR_CHECK(esp_wifi_connect());
    }

    // WiFi任务主循环
    while (1) {
        // 在这里可以添加WiFi状态检查、重连逻辑等
        vTaskDelay(pdMS_TO_TICKS(5000)); // 每5秒检查一次
    }

    // 正常情况下不会执行到这里
    vTaskDelete(NULL);
}

/**
 * @brief 获取当前WiFi模式
 * @param mode 用于存储WiFi模式的指针
 * @return ESP_OK 成功，其他失败
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
 * @return true 已连接，false 未连接
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
 * @return ESP_OK 成功，其他失败
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
 * @brief 切换WiFi开关
 * @param enable true 启用WiFi，false 禁用WiFi
 * @return ESP_OK 成功，其他失败
 */
// 保存WiFi模式的全局变量
static wifi_mode_t saved_wifi_mode = WIFI_MODE_NULL;

esp_err_t wifi_toggle(bool enable)
{
    if (enable) {
        // 如果是从关闭状态开启WiFi
        if (saved_wifi_mode == WIFI_MODE_NULL) {
            // 设置为AP+STA模式（与初始化一致）
            esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
            if (ret != ESP_OK) {
                return ret;
            }
            
            // 启动WiFi
            return esp_wifi_start();
        } else {
            // 先设置之前保存的模式
            esp_err_t ret = esp_wifi_set_mode(saved_wifi_mode);
            if (ret != ESP_OK) {
                return ret;
            }
            
            // 启动WiFi
            return esp_wifi_start();
        }
    } else {
        // 保存当前WiFi模式
        esp_wifi_get_mode(&saved_wifi_mode);
        
        // 停止WiFi
        esp_err_t ret = esp_wifi_stop();
        if (ret == ESP_OK) {
            // 明确设置WiFi模式为NULL，确保状态正确反映
            esp_wifi_set_mode(WIFI_MODE_NULL);
        }
        return ret;
    }
}

/**
 * @brief 获取HTTP服务器端口号
 * @return HTTP服务器端口号
 */
uint16_t wifi_get_http_port(void)
{
    // 默认端口号，与HTTP服务器配置一致
    return 80;
}

/**
 * @brief WiFi任务启动函数
 * 创建并启动WiFi管理任务
 */
void wifi_task(void)
{
    xTaskCreate(app_wifi_task, "wifi_task", 4 * 1024, NULL, 4, NULL);
}


