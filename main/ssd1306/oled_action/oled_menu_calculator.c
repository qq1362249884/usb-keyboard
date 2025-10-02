/*
 * OLED菜单计算器功能实现
 * 提供基本的计算器功能，支持加减乘除运算
 */

// 标准库头文件
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>

// ESP-IDF组件头文件
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// 项目内部头文件
#include "oled_menu_combined.h"
#include "oled_menu_display.h"
#include "spi_scanner/keycodes.h"
#include "tinyusb_hid/tinyusb_hid.h"
#include "OLED_driver.h"
#include "OLED.h"

// 计算器状态定义
typedef struct {
    char current_value_str[64];  // 当前显示值的字符串表示
    char stored_value_str[64];   // 存储值的字符串表示
    char operation;              // 当前操作符：+、-、*、/
    bool new_input;             // 是否为新输入
    bool operation_pending;     // 是否有待执行的操作
    bool error_state;           // 错误状态
    bool decimal_mode;          // 是否处于小数输入模式
    bool force_scientific_notation; // 强制使用科学计数法显示
    int decimal_places;         // 小数位数计数
    int integer_digits;         // 整数部分位数计数
} calculator_state_t;

// 全局计算器状态
static calculator_state_t calc_state = {
    .current_value_str = "0",
    .stored_value_str = "0",
    .operation = '\0',
    .new_input = true,
    .operation_pending = false,
    .error_state = false,
    .decimal_mode = false,
    .force_scientific_notation = false,
    .decimal_places = 0,
    .integer_digits = 0
};

// 从oled_menu_display.c获取的函数声明
extern QueueHandle_t get_keyboard_queue(void);
extern QueueHandle_t get_joystick_queue(void);
extern MenuManager* get_menu_manager(void);

// 函数声明
static bool is_input_overflow(const char* value_str);
static void string_to_double_for_display(const char* str, double* result);

/**
 * @brief 处理数字按键输入
 * @param digit 数字值（0-9）
 */
static void handle_digit_input(uint8_t digit) {
    if (calc_state.error_state) {
        return; // 错误状态下不接受输入
    }
    
    // 检查输入位数限制（最大支持16位数字）
    int current_len = strlen(calc_state.current_value_str);
    if (current_len >= 16) {
        return; // 超过最大位数，拒绝输入
    }
    
    if (calc_state.new_input) {
        // 新输入：直接设置数字
        if (digit == 0) {
            strcpy(calc_state.current_value_str, "0");
        } else {
            snprintf(calc_state.current_value_str, sizeof(calc_state.current_value_str), "%d", digit);
        }
        calc_state.new_input = false;
        calc_state.decimal_mode = false;
        calc_state.decimal_places = 0;
        calc_state.integer_digits = 1;
    } else {
        // 继续输入
        char new_str[64];
        if (calc_state.decimal_mode) {
            // 小数输入模式：直接追加数字
            int len = snprintf(new_str, sizeof(new_str), "%s%d", calc_state.current_value_str, digit);
            if (len >= (int)sizeof(new_str)) {
                return; // 缓冲区溢出，拒绝输入
            }
            calc_state.decimal_places++;
        } else {
            // 整数输入模式：直接追加数字
            if (strcmp(calc_state.current_value_str, "0") == 0) {
                int len = snprintf(new_str, sizeof(new_str), "%d", digit);
                if (len >= (int)sizeof(new_str)) {
                    return; // 缓冲区溢出，拒绝输入
                }
            } else {
                int len = snprintf(new_str, sizeof(new_str), "%s%d", calc_state.current_value_str, digit);
                if (len >= (int)sizeof(new_str)) {
                    return; // 缓冲区溢出，拒绝输入
                }
            }
            calc_state.integer_digits++;
        }
        
        // 检查是否超出显示范围
        if (!is_input_overflow(new_str)) {
            strcpy(calc_state.current_value_str, new_str);
        }
    }
}



// 辅助函数：字符串转double，用于显示
static void string_to_double_for_display(const char* str, double* result) {
    // 对于大数，使用科学计数法近似表示
    if (strlen(str) > 15) {
        // 大数：使用对数方法计算近似值
        double value = atof(str);
        if (value != 0.0) {
            double log_value = log10(fabs(value));
            double mantissa = pow(10.0, log_value - floor(log_value));
            int exponent = (int)floor(log_value);
            *result = (value < 0 ? -1.0 : 1.0) * mantissa * pow(10.0, exponent);
        } else {
            *result = 0.0;
        }
    } else {
        // 小数：直接转换
        *result = atof(str);
    }
}

// 辅助函数：检查输入是否超出显示范围
static bool is_input_overflow(const char* value_str) {
    // 检查字符串长度是否超过最大限制
    if (strlen(value_str) > 16) {
        return true;
    }
    
    // 检查是否包含非数字字符（除了小数点和负号）
    for (const char* p = value_str; *p; p++) {
        if (!((*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) {
            return true;
        }
    }
    
    return false;
}

// 辅助函数：字符串减法运算，实现精确的大数减法运算
static void string_subtract(const char* num1, const char* num2, char* result, size_t result_size) {
    if (result_size < 2) {
        strcpy(result, "0");
        return;
    }
    
    int len1 = strlen(num1);
    int len2 = strlen(num2);
    int max_len = (len1 > len2) ? len1 : len2;
    
    // 检查结果缓冲区是否足够大
    if (result_size < (size_t)(max_len + 2)) {
        strcpy(result, "overflow");
        return;
    }
    
    // 分配临时缓冲区
    char* temp_result = malloc(max_len + 2);
    if (!temp_result) {
        strcpy(result, "Error");
        return;
    }
    
    memset(temp_result, '0', max_len + 1);
    temp_result[max_len + 1] = '\0';
    
    int borrow = 0;
    
    // 从最低位开始相减
    for (int i = 0; i < max_len; i++) {
        int digit1 = (i < len1) ? (num1[len1 - 1 - i] - '0') : 0;
        int digit2 = (i < len2) ? (num2[len2 - 1 - i] - '0') : 0;
        
        digit1 -= borrow;
        
        if (digit1 < digit2) {
            digit1 += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }
        
        int diff = digit1 - digit2;
        temp_result[max_len - i] = diff + '0';
    }
    
    // 处理结果为负数的情况
    if (borrow > 0) {
        strcpy(result, "Error"); // 结果为负，显示错误
    } else {
        // 去除前导零
        char* start = temp_result;
        while (*start == '0' && *(start + 1) != '\0') {
            start++;
        }
        strcpy(result, start);
    }
    
    free(temp_result);
}

// 辅助函数：字符串加法，实现精确的大数加法运算
static void string_add(const char* num1, const char* num2, char* result, size_t result_size) {
    if (result_size < 2) {
        strcpy(result, "0");
        return;
    }
    
    int len1 = strlen(num1);
    int len2 = strlen(num2);
    int max_len = (len1 > len2) ? len1 : len2;
    
    // 检查结果缓冲区是否足够大
    if (result_size < (size_t)(max_len + 2)) {
        strcpy(result, "overflow");
        return;
    }
    
    // 分配临时缓冲区
    char* temp_result = malloc(max_len + 2);
    if (!temp_result) {
        strcpy(result, "Error");
        return;
    }
    
    memset(temp_result, '0', max_len + 1);
    temp_result[max_len + 1] = '\0';
    
    int carry = 0;
    
    // 从最低位开始相加
    for (int i = 0; i < max_len; i++) {
        int digit1 = (i < len1) ? (num1[len1 - 1 - i] - '0') : 0;
        int digit2 = (i < len2) ? (num2[len2 - 1 - i] - '0') : 0;
        
        int sum = digit1 + digit2 + carry;
        carry = sum / 10;
        temp_result[max_len - i] = (sum % 10) + '0';
    }
    
    // 处理最高位的进位
    if (carry > 0) {
        temp_result[0] = carry + '0';
        strcpy(result, temp_result);
    } else {
        strcpy(result, temp_result + 1);
    }
    
    free(temp_result);
}

// 辅助函数：字符串乘法，实现精确的大数乘法运算
static void string_multiply(const char* num1, const char* num2, char* result, size_t result_size) {
    // 处理符号
    int sign1 = 1, sign2 = 1;
    const char* p1 = num1;
    const char* p2 = num2;
    
    if (*p1 == '-') {
        sign1 = -1;
        p1++;
    }
    if (*p2 == '-') {
        sign2 = -1;
        p2++;
    }
    
    // 跳过前导零
    while (*p1 == '0' && *(p1 + 1) != '\0') p1++;
    while (*p2 == '0' && *(p2 + 1) != '\0') p2++;
    
    // 计算数字长度
    int len1 = strlen(p1);
    int len2 = strlen(p2);
    
    // 如果数字太长，使用科学计数法近似（16+16=32，需要精确计算）
    if (len1 + len2 > 32) {
        // 使用对数方法计算近似值
        double val1 = atof(num1);
        double val2 = atof(num2);
        double result_val = val1 * val2;
        
        // 格式化为科学计数法
        if (result_val == 0.0) {
            strncpy(result, "0", result_size);
        } else {
            snprintf(result, result_size, "%.2e", result_val);
        }
        return;
    }
    
    // 分配结果数组（最大可能长度是len1+len2）
    int max_len = len1 + len2;
    int* res = calloc(max_len, sizeof(int));
    
    // 执行乘法运算（从低位到高位）
    for (int i = len1 - 1; i >= 0; i--) {
        for (int j = len2 - 1; j >= 0; j--) {
            int digit1 = p1[i] - '0';
            int digit2 = p2[j] - '0';
            int product = digit1 * digit2;
            
            int pos1 = i + j;
            int pos2 = i + j + 1;
            
            int sum = product + res[pos2];
            res[pos2] = sum % 10;
            res[pos1] += sum / 10;
        }
    }
    
    // 处理进位
    for (int i = max_len - 1; i > 0; i--) {
        if (res[i] >= 10) {
            res[i-1] += res[i] / 10;
            res[i] %= 10;
        }
    }
    
    // 转换为字符串
    int start_index = 0;
    while (start_index < max_len - 1 && res[start_index] == 0) {
        start_index++;
    }
    
    int result_len = max_len - start_index;
    if (result_len + 1 > (int)result_size) {
        // 结果太长，直接返回overflow
        strncpy(result, "overflow", result_size);
        free(res);
        return;
    }
    
    // 构建结果字符串
    int idx = 0;
    if (sign1 * sign2 == -1) {
        result[idx++] = '-';
    }
    
    for (int i = start_index; i < max_len; i++) {
        result[idx++] = res[i] + '0';
    }
    result[idx] = '\0';
    
    free(res);
}

/**
 * @brief 执行待处理的操作
 */
static void execute_pending_operation(void) {
    if (!calc_state.operation_pending) {
        return;
    }
    
    // 将字符串转换为double进行运算（仅用于显示）
    double stored_value, current_value;
    string_to_double_for_display(calc_state.stored_value_str, &stored_value);
    string_to_double_for_display(calc_state.current_value_str, &current_value);
    
    double result_value = 0.0;
    
    // 获取操作数长度（用于字符串运算判断）
    int len1 = strlen(calc_state.stored_value_str);
    int len2 = strlen(calc_state.current_value_str);
    
    switch (calc_state.operation) {
        case '+':
            // 字符串加法运算：避免使用double类型
            // 检查操作数长度（使用已定义的len1和len2）
            
            // 如果两个操作数都很长（超过8位），使用字符串加法
            if (len1 > 8 || len2 > 8) {
                calc_state.force_scientific_notation = true;
                
                // 使用字符串加法计算精确结果
                char result_str[64] = {0};
                string_add(calc_state.stored_value_str, calc_state.current_value_str, result_str, sizeof(result_str));
                
                // 检查结果是否包含overflow
                if (strcmp(result_str, "overflow") == 0) {
                    // 结果太大，显示overflow
                    strcpy(calc_state.current_value_str, "overflow");
                    calc_state.error_state = true;
                } else {
                    // 直接使用字符串结果
                    strcpy(calc_state.current_value_str, result_str);
                }
                
                calc_state.operation_pending = false;
                calc_state.operation = '\0';
                return; // 直接返回，跳过后续的double处理
            } else {
                // 对于较短的数，使用精确计算
                calc_state.force_scientific_notation = false;
                result_value = stored_value + current_value;
            }
            break;
        case '-':
             // 字符串减法运算：避免使用double类型
             // 检查操作数长度
             len1 = strlen(calc_state.stored_value_str);
             len2 = strlen(calc_state.current_value_str);
             
             // 如果两个操作数都很长（超过8位），使用字符串减法
             if (len1 > 8 || len2 > 8) {
                 calc_state.force_scientific_notation = true;
                 
                 // 使用字符串减法计算精确结果
                 char result_str[64] = {0};
                 string_subtract(calc_state.stored_value_str, calc_state.current_value_str, result_str, sizeof(result_str));
                 
                 // 检查结果是否包含Error（表示负数）
                 if (strcmp(result_str, "Error") == 0) {
                     // 结果为负，显示Error
                     strcpy(calc_state.current_value_str, "Error");
                     calc_state.error_state = true;
                 } else if (strcmp(result_str, "overflow") == 0) {
                     // 结果太大，显示overflow
                     strcpy(calc_state.current_value_str, "overflow");
                     calc_state.error_state = true;
                 } else {
                     // 直接使用字符串结果
                     strcpy(calc_state.current_value_str, result_str);
                 }
                 
                 calc_state.operation_pending = false;
                 calc_state.operation = '\0';
                 return; // 直接返回，跳过后续的double处理
             } else {
                 // 对于较短的数，使用精确计算
                 calc_state.force_scientific_notation = false;
                 result_value = stored_value - current_value;
             }
             break;
        case '*':
            // 字符串乘法运算：避免使用double类型
            // 检查操作数长度（使用已定义的len1和len2）
            
            // 如果两个操作数都很长（超过8位），使用字符串乘法
            if (len1 > 8 || len2 > 8) {
                calc_state.force_scientific_notation = true;
                
                // 使用字符串乘法计算精确结果
                char result_str[64] = {0};
                string_multiply(calc_state.stored_value_str, calc_state.current_value_str, result_str, sizeof(result_str));
                
                // 检查结果是否包含科学计数法（表示溢出）
                if (strchr(result_str, 'e') != NULL || strchr(result_str, 'E') != NULL) {
                    // 结果太大，显示overflow
                    strcpy(calc_state.current_value_str, "overflow");
                    calc_state.error_state = true;
                } else {
                    // 直接使用字符串结果
                    strcpy(calc_state.current_value_str, result_str);
                }
                
                calc_state.operation_pending = false;
                calc_state.operation = '\0';
                return; // 直接返回，跳过后续的double处理
            } else {
                // 对于较短的数，使用精确计算
                calc_state.force_scientific_notation = false;
                result_value = stored_value * current_value;
            }
            break;
        case '/':
            if (current_value == 0) {
                calc_state.error_state = true;
                result_value = 0;
            } else {
                result_value = stored_value / current_value;
            }
            break;
        default:
            break;
    }
    
    // 将结果转换回字符串存储
    if (calc_state.error_state) {
        strcpy(calc_state.current_value_str, "Error");
    } else if (calc_state.force_scientific_notation) {
        // 科学计数法格式
        if (result_value == 0.0) {
            strcpy(calc_state.current_value_str, "0");
        } else {
            double abs_value = fabs(result_value);
            if (abs_value >= 1e10 || abs_value < 1e-10) {
                snprintf(calc_state.current_value_str, sizeof(calc_state.current_value_str), "%.2e", result_value);
            } else {
                snprintf(calc_state.current_value_str, sizeof(calc_state.current_value_str), "%.6e", result_value);
            }
        }
    } else {
        // 普通格式
        if (result_value == (int64_t)result_value) {
            snprintf(calc_state.current_value_str, sizeof(calc_state.current_value_str), "%.0f", result_value);
        } else {
            snprintf(calc_state.current_value_str, sizeof(calc_state.current_value_str), "%.2f", result_value);
        }
    }
    
    calc_state.operation_pending = false;
    calc_state.operation = '\0';
}

/**
 * @brief 处理操作符输入
 * @param op 操作符：+、-、*、/
 */
static void handle_operation(char op) {
    if (calc_state.error_state) {
        return;
    }
    
    if (calc_state.operation_pending) {
        // 检查是否已经输入了第二个操作数
        // 如果当前值不是"0"且不是新输入状态，说明已经输入了第二个操作数，不能更换操作符
        if (strcmp(calc_state.current_value_str, "0") != 0 && !calc_state.new_input) {
            // 已经输入了第二个操作数，不能更换操作符，直接返回
            return;
        }
        
        // 还没有输入第二个操作数，可以更换操作符
        calc_state.operation = op;
        return;
    }
    
    // 存储当前值到存储值
    strcpy(calc_state.stored_value_str, calc_state.current_value_str);
    
    // 清除当前显示值，准备接收新输入
    strcpy(calc_state.current_value_str, "0");
    calc_state.operation = op;
    calc_state.operation_pending = true;
    calc_state.new_input = true;
    calc_state.decimal_mode = false;
    calc_state.force_scientific_notation = false; // 重置科学计数法标志
    calc_state.decimal_places = 0;
    calc_state.integer_digits = 0; // 重置整数位数计数
}

/**
 * @brief 处理等号输入
 */
static void handle_equals(void) {
    if (calc_state.error_state) {
        return;
    }
    
    if (calc_state.operation_pending) {
        execute_pending_operation();
    }
    
    calc_state.new_input = true;
    calc_state.operation_pending = false;
    calc_state.operation = '\0';
    calc_state.decimal_mode = false;
    calc_state.force_scientific_notation = false; // 重置科学计数法标志
    calc_state.decimal_places = 0;
    calc_state.integer_digits = 0; // 重置整数位数计数
}

/**
 * @brief 处理小数点输入
 */
static void handle_decimal_point(void) {
    if (calc_state.error_state) {
        return; // 错误状态下不接受输入
    }
    
    if (!calc_state.decimal_mode) {
        // 进入小数输入模式
        calc_state.decimal_mode = true;
        calc_state.decimal_places = 0;
        
        // 如果是新输入，将当前值设为0
        if (calc_state.new_input) {
            strcpy(calc_state.current_value_str, "0");
            calc_state.new_input = false;
        }
        
        // 添加小数点
        if (strchr(calc_state.current_value_str, '.') == NULL) {
            strcat(calc_state.current_value_str, ".");
        }
    }
    // 如果已经处于小数模式，忽略重复的小数点输入
}

/**
 * @brief 清除计算器状态
 */
static void handle_clear(void) {
    strcpy(calc_state.current_value_str, "0");
    strcpy(calc_state.stored_value_str, "0");
    calc_state.operation = '\0';
    calc_state.new_input = true;
    calc_state.operation_pending = false;
    calc_state.error_state = false;
    calc_state.decimal_mode = false;
    calc_state.force_scientific_notation = false; // 重置科学计数法标志
    calc_state.decimal_places = 0;
    calc_state.integer_digits = 0; // 重置整数位数计数
}

/**
 * @brief 格式化显示数值
 * @param value_str 要格式化的数值字符串
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 */
static void format_display_value(const char* value_str, char* buffer, size_t buffer_size) {
    if (calc_state.error_state) {
        strncpy(buffer, "Error", buffer_size);
        return;
    }
    
    // 检查是否需要强制使用科学计数法
    if (calc_state.force_scientific_notation) {
        // 对于科学计数法模式，直接使用存储的字符串
        strncpy(buffer, value_str, buffer_size);
        return;
    }
    
    if (calc_state.decimal_mode) {
        // 小数输入模式：直接显示字符串
        strncpy(buffer, value_str, buffer_size);
    } else {
        // 普通模式：直接显示字符串
        strncpy(buffer, value_str, buffer_size);
    }
}



/**
 * @brief 显示计算器界面
 */
static void display_calculator(void) {
    OLED_Clear();
    
    // 顶部状态栏 - 类似手机计算器
    OLED_DrawLine(0, 0, 127, 0); // 顶部边框
    OLED_ShowString(2, 1,"Calculator", OLED_6X8_HALF);

    // 显示区域背景 - 类似手机计算器的显示框
    OLED_DrawRectangle(2, 10, 124, 18, OLED_UNFILLED); // 显示框边框
    
    // 左右外框 - 增强视觉效果
    OLED_DrawLine(0, 0, 0, 27); // 左外框（延伸到顶部）
    OLED_DrawLine(127, 0, 127, 27); // 右外框（延伸到顶部）
    
    // 显示当前值 - 类似手机计算器的显示区域
    char display_buffer[64];
    format_display_value(calc_state.current_value_str, display_buffer, sizeof(display_buffer));
    
    // 计算文本宽度以右对齐显示（类似手机计算器）
    int text_width = strlen(display_buffer) * 8; // OLED_8X16_HALF字体宽度为8px
    int start_x = 124 - text_width; // 右对齐，留4px边距
    
    // 字体缩放逻辑：如果文本接近显示框边缘，使用小一号字体
    bool use_small_font = false;
    if (start_x < 6) {
        // 使用小字体重新计算
        use_small_font = true;
        text_width = strlen(display_buffer) * 6; // OLED_6X8_HALF字体宽度为6px
        start_x = 124 - text_width;
        if (start_x < 6) start_x = 6; // 最小边距
    }
    
    // 如果使用小字体后仍然超出显示框，使用科学计数法显示
    if (text_width > (124 - 6)) {
        // 将长数字转换为科学计数法显示
        double value = atof(display_buffer);
        if (value != 0.0) {
            snprintf(display_buffer, sizeof(display_buffer), "%.2e", value);
        }
        
        // 重新计算显示位置
        text_width = strlen(display_buffer) * 8;
        start_x = 124 - text_width;
        if (start_x < 6) start_x = 6;
        use_small_font = false;
    }
    
    // 显示当前值（根据情况选择字体大小）
    if (use_small_font) {
        OLED_ShowString(start_x, 12, display_buffer, OLED_6X8_HALF);
    } else {
        OLED_ShowString(start_x, 11, display_buffer, OLED_8X16_HALF);
    }
    
    // 显示当前操作符（如果有）- 放在显示值的左侧
    if (calc_state.operation != '\0' && !calc_state.force_scientific_notation) {
        char op_buffer[4];
        snprintf(op_buffer, sizeof(op_buffer), "%c", calc_state.operation);
        int op_x = start_x - 12;
        if (use_small_font) {
            op_x = start_x - 10;
            OLED_ShowString(op_x, 12, op_buffer, OLED_6X8_HALF);
        } else {
            OLED_ShowString(op_x, 11, op_buffer, OLED_8X16_HALF);
        }
    }
    
    // 底部状态栏 - 简洁设计
    OLED_DrawLine(0, 28, 127, 28); // 分隔线
    
    OLED_Update();
}

/**
 * @brief 将按键码转换为计算器操作
 * @param keycode 按键码
 * @return true 成功处理
 * @return false 未处理的按键
 */
static bool process_calculator_key(uint16_t keycode) {
    switch (keycode) {
        // 数字按键 - 只处理映射层0中存在的小键盘数字键
        case KC_KP_0: handle_digit_input(0); return true;
        case KC_KP_1: handle_digit_input(1); return true;
        case KC_KP_2: handle_digit_input(2); return true;
        case KC_KP_3: handle_digit_input(3); return true;
        case KC_KP_4: handle_digit_input(4); return true;
        case KC_KP_5: handle_digit_input(5); return true;
        case KC_KP_6: handle_digit_input(6); return true;
        case KC_KP_7: handle_digit_input(7); return true;
        case KC_KP_8: handle_digit_input(8); return true;
        case KC_KP_9: handle_digit_input(9); return true;
        
        // 操作符按键 - 只处理映射层0中存在的操作符键
        case KC_KP_PLUS: // 加号
            handle_operation('+'); return true;
        case KC_KP_MINUS: // 减号
            handle_operation('-'); return true;
        case KC_KP_ASTERISK: // 乘号
            handle_operation('*'); return true;
        case KC_KP_SLASH: // 除号
            handle_operation('/'); return true;
        
        // 功能按键 - 只处理映射层0中存在的功能键
        case KC_KP_ENTER: // 等号
            handle_equals(); return true;
        case KC_ESCAPE: // 清除
            handle_clear(); return true;
        case KC_KP_DOT: // 小键盘小数点
            handle_decimal_point(); return true;
        
        default:
            return false;
    }
}

/**
 * @brief 计算器功能主函数
 * 使用键盘按键进行输入，在使用计算器时中断tinyusb的发送报告
 */
void menuActionCalculator(void) {
    // 主循环
    bool exit_calculator = false;
    uint16_t last_keycode = 0;
    TickType_t last_key_time = 0;
    const TickType_t debounce_delay = 200 / portTICK_PERIOD_MS; // 200ms消抖延时
        // 保存当前映射层
    extern uint8_t current_keymap_layer;
    uint8_t original_layer = current_keymap_layer;
    // 切换到映射层0（小键盘布局）
    current_keymap_layer = 0;

    OLED_Clear();
    // 初始化计算器状态
    handle_clear();
    // 禁用HID报告发送，避免计算器输入被发送到主机
    tinyusb_hid_enable_report(false);
    // 显示计算器界面
    display_calculator();
    

    
    while (!exit_calculator ) {
        uint16_t keycode = 0;
        uint8_t joystick_op = 0;
        
        // 同时监听键盘队列和摇杆队列
        if (xQueueReceive(get_keyboard_queue(), &keycode, 0) == pdTRUE) {
            TickType_t current_time = xTaskGetTickCount();
            
            // 按键消抖处理：相同按键在消抖时间内只处理一次
            if (keycode == last_keycode && (current_time - last_key_time) < debounce_delay) {
                continue; // 忽略重复按键
            }
            
            last_keycode = keycode;
            last_key_time = current_time;
            
            // 处理计算器按键
            if (process_calculator_key(keycode)) {
                display_calculator();
            }
        }
        
        // 独立监听摇杆队列，确保及时处理摇杆事件
        if (xQueueReceive(get_joystick_queue(), &joystick_op, 0) == pdTRUE) {
            // 处理摇杆队列事件
            if (joystick_op == MENU_OP_BACK) {
                // 摇杆长按或双击事件，退出计算器
                exit_calculator = true;
            }
        }
        
        // 添加短暂延时，避免过度占用CPU
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    // 重新启用HID报告发送
    tinyusb_hid_enable_report(true);
    
    // 恢复原始映射层
    current_keymap_layer = original_layer;
    
    // 显示退出信息
    OLED_Clear();
    OLED_ShowString(44, 12, "Exiting", OLED_8X16_HALF);
    OLED_Update();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    // 返回主菜单
    MenuManager_DisplayMenu(get_menu_manager(), 0, 0, OLED_8X16_HALF);
}

/**
 * @brief 获取计算器状态（用于测试）
 * @return calculator_state_t* 计算器状态指针
 */
calculator_state_t* get_calculator_state(void) {
    return &calc_state;
}

/**
 * @brief 重置计算器状态（用于测试）
 */
void reset_calculator_state(void) {
    handle_clear();
}