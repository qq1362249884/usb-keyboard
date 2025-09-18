/*
 * init_app.h
 * 初始化应用程序头文件
 * 声明应用程序初始化函数和相关接口
 */
#ifndef MAIN_INIT_APP_H_
#define MAIN_INIT_APP_H_

#include "esp_err.h"

/**
 * @brief 应用程序初始化函数
 * 使用初始化管理器协调所有模块的初始化顺序
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t app_init(void);

#endif /* MAIN_INIT_APP_H_ */