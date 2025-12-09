#include "joystick.h"



//全局变量定义
static const char *TAG = "app_joystick";


// 中值滤波参数定义
#define FILTER_WINDOW_SIZE  5       // 减小滤波窗口大小以提高响应速度
#define NUM_ADC_CHANNELS    2       // ADC通道数量

// 按键检测参数定义
#define SHORT_PRESS_TIMEOUT_MS 500  // 短按超时时间
#define LONG_PRESS_TIMEOUT_MS 1000  // 长按超时时间
#define DOUBLE_PRESS_TIMEOUT_MS 400 // 双击超时时间，已延长以提高双击识别率

// ADC句柄和校准句柄
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;

// 滤波缓冲区和索引
static int adc_filter_buffers[NUM_ADC_CHANNELS][FILTER_WINDOW_SIZE] = {0};
static int adc_filter_index[NUM_ADC_CHANNELS] = {0};
static int adc_filter_initialized = 0;

// 按键状态变量
static int32_t button_press_start_time = 0;
static int32_t last_button_release_time = 0;
static bool button_pressed = false;
static bool button_released = false;
static int press_count = 0;
static button_press_type_t detected_press_type = BUTTON_NONE;

void sw_gpio_init(void) {
    gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << JOYSTICK_SW_PIN,
                            .mode = GPIO_MODE_INPUT,
                            .pull_down_en = 0,
                            .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
    
    // 初始化ADC
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    
    // 配置ADC通道
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_1, &channel_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_2, &channel_config));
    
    // 初始化ADC校准
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle));
}

// 中值滤波函数
static int median_filter(int channel_idx, int new_value) {
    // 初始化缓冲区（首次调用时）
    if (!adc_filter_initialized) {
        // 使用更稳健的方式初始化：先收集一定数量的有效样本
        static int init_count[NUM_ADC_CHANNELS] = {0};
        if (init_count[channel_idx] < FILTER_WINDOW_SIZE) {
            adc_filter_buffers[channel_idx][init_count[channel_idx]] = new_value;
            init_count[channel_idx]++;
            if (init_count[0] >= FILTER_WINDOW_SIZE && init_count[1] >= FILTER_WINDOW_SIZE) {
                adc_filter_initialized = 1;
            }
            return new_value;
        }
    }

    // 替换最旧的值
    adc_filter_buffers[channel_idx][adc_filter_index[channel_idx]] = new_value;
    adc_filter_index[channel_idx] = (adc_filter_index[channel_idx] + 1) % FILTER_WINDOW_SIZE;

    // 创建临时数组用于排序
    int temp_buffer[FILTER_WINDOW_SIZE];
    for (int i = 0; i < FILTER_WINDOW_SIZE; i++) {
        temp_buffer[i] = adc_filter_buffers[channel_idx][i];
    }

    // 冒泡排序
    for (int i = 0; i < FILTER_WINDOW_SIZE - 1; i++) {
        for (int j = 0; j < FILTER_WINDOW_SIZE - i - 1; j++) {
            if (temp_buffer[j] > temp_buffer[j + 1]) {
                // 交换元素
                int temp = temp_buffer[j];
                temp_buffer[j] = temp_buffer[j + 1];
                temp_buffer[j + 1] = temp;
            }
        }
    }

    // 返回中值
    return temp_buffer[FILTER_WINDOW_SIZE / 2];
}


// 获取摇杆方向和旋转状态
// 用于跟踪最后摇杆方向的静态变量
static joystick_direction_t last_joystick_direction = JOYSTICK_CENTER;

joystick_state_t get_joystick_direction()
{
    joystick_state_t state;
    
    // 读取原始ADC值
    int raw_1, raw_2;
    esp_err_t ret1 = adc_oneshot_read(adc1_handle, ADC_CHANNEL_1, &raw_1);
    esp_err_t ret2 = adc_oneshot_read(adc1_handle, ADC_CHANNEL_2, &raw_2);
    vTaskDelay(5 / portTICK_PERIOD_MS);

    // 处理ADC读取错误
    if (ret1 != ESP_OK || ret2 != ESP_OK) {
        ESP_LOGW(TAG, "ADC read failed: %s, %s", esp_err_to_name(ret1), esp_err_to_name(ret2));
        // 返回中心位置，避免系统崩溃
        state.direction = JOYSTICK_CENTER;
        state.press_type = detect_button_press();
        return state;
    }

    // 应用中值滤波
    int filtered_1 = median_filter(0, raw_1);
    int filtered_2 = median_filter(1, raw_2);

    // ESP_LOGI(TAG, "ADC原始值: %d, %d | 滤波后值: %d, %d", raw_1, raw_2, filtered_1, filtered_2);

    // 检测按键按下的类型
    state.press_type = detect_button_press();

    // 如果滤波器未初始化完成，返回中心位置
    if (!adc_filter_initialized) {
        state.direction = JOYSTICK_CENTER;
        return state;
    }

    // 根据实际ADC值动态调整判断逻辑，提高准确性和灵活性
    // (4095,2183)为上、(0,2183)为下、(2041,0)为左、(2041,4095)为右
    // 定义各方向的参考值
    const int up_x = 4095;
    const int up_y = 2183;
    const int down_x = 0;
    const int down_y = 2183;
    const int left_x = 2041;
    const int left_y = 0;
    const int right_x = 2041;
    const int right_y = 4095;
    
    // 使用欧几里得距离判断最接近的方向
    int distance_up = (filtered_1 - up_x) * (filtered_1 - up_x) + (filtered_2 - up_y) * (filtered_2 - up_y);
    int distance_down = (filtered_1 - down_x) * (filtered_1 - down_x) + (filtered_2 - down_y) * (filtered_2 - down_y);
    int distance_left = (filtered_1 - left_x) * (filtered_1 - left_x) + (filtered_2 - left_y) * (filtered_2 - left_y);
    int distance_right = (filtered_1 - right_x) * (filtered_1 - right_x) + (filtered_2 - right_y) * (filtered_2 - right_y);
    
    // 定义一个阈值，当距离小于该阈值时才认为是有效方向
    const int threshold = 200 * 200; // 增大阈值以提高灵敏度
    
    // 如果所有方向的距离都大于阈值，则返回中心位置
    if (distance_up > threshold && distance_down > threshold && distance_left > threshold && distance_right > threshold) {
        state.direction = JOYSTICK_CENTER;
    } else {
        // 找到距离最小的方向
        int min_distance = distance_up;
        state.direction = JOYSTICK_UP;
        
        if (distance_down < min_distance) {
            min_distance = distance_down;
            state.direction = JOYSTICK_DOWN;
        }
        
        if (distance_left < min_distance) {
            min_distance = distance_left;
            state.direction = JOYSTICK_LEFT;
        }
        
        if (distance_right < min_distance) {
            min_distance = distance_right;
            state.direction = JOYSTICK_RIGHT;
        }
    }
    
    // 如果摇杆方向改变，更新最后活动时间以防止设备进入睡眠模式
    if (state.direction != last_joystick_direction) {
        update_last_activity_time();
        last_joystick_direction = state.direction;
    }
    
    return state;
}

// 检测按键按下的类型
button_press_type_t detect_button_press() {
    int current_sw_state = (gpio_get_level(JOYSTICK_SW_PIN) == 0);
    int64_t current_time = esp_timer_get_time() / 1000; // 转换为毫秒

    // 检测按键按下
    if (current_sw_state && !button_pressed) {
        button_pressed = true;
        button_press_start_time = current_time;
        button_released = false;
    }
    
    // 检测长按事件（在按键按下期间检测）
    if (button_pressed && !button_released) {
        int64_t press_duration = current_time - button_press_start_time;
        if (press_duration >= LONG_PRESS_TIMEOUT_MS && detected_press_type != BUTTON_LONG_PRESS) {
            detected_press_type = BUTTON_LONG_PRESS;
            // 不重置计数器，允许在释放后继续处理短按或双击
        }
    }
    
    // 检测按键释放
    if (!current_sw_state && button_pressed) {
        button_pressed = false;
        button_released = true;
        last_button_release_time = current_time;
        
        // 只有在未检测到长按的情况下才增加按压计数
        if (detected_press_type != BUTTON_LONG_PRESS) {
            press_count++;
        }
    
        // 如果之前未检测到长按，则检查短按或双击
        if (detected_press_type != BUTTON_LONG_PRESS) {
            if (press_count == 1) {
                // 等待可能的双击
                // 不立即返回，等待超时或再次按下
            } else if (press_count == 2) {
                detected_press_type = BUTTON_DOUBLE_PRESS;
                press_count = 0; // 重置计数器
            }
        }
    }

    // 处理短按超时
    if (button_released && (current_time - last_button_release_time >= SHORT_PRESS_TIMEOUT_MS)) {
        if (press_count == 1) {
            // 只有在未检测到长按的情况下才触发短按
            if (detected_press_type != BUTTON_LONG_PRESS) {
                detected_press_type = BUTTON_SHORT_PRESS;
            }
            press_count = 0; // 重置计数器
        }
        button_released = false; // 重置释放状态
    }

    // 处理双击超时
    if (press_count == 1 && (current_time - last_button_release_time >= DOUBLE_PRESS_TIMEOUT_MS)) {
        // 只有在未检测到长按的情况下才触发短按（作为双击超时后的单击）
        if (detected_press_type != BUTTON_LONG_PRESS) {
            detected_press_type = BUTTON_SHORT_PRESS;
        }
        press_count = 0; // 重置计数器
    }

    button_press_type_t result = detected_press_type;
    detected_press_type = BUTTON_NONE; // 清除已检测到的按键类型
    return result;
}

/**
 * @brief 更新最后活动时间，防止设备进入睡眠模式
 * 这是一个空函数，用于防止编译错误，实际功能需要根据具体需求实现
 */
void update_last_activity_time() {
    // 这里可以添加实际的最后活动时间更新逻辑
    // 例如：更新一个全局的时间戳变量
    // 目前先实现为空函数以通过编译
}

