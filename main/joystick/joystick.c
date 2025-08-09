#include "joystick.h"

//全局变量定义
static const char *TAG = "app_joystick";

#define DEFAULT_VREF    1100        //默认参考电压，单位mV

// 中值滤波参数定义
#define FILTER_WINDOW_SIZE  11       // 滤波窗口大小（中值滤波通常使用较小窗口）
#define NUM_ADC_CHANNELS    2       // ADC通道数量

// 滤波缓冲区和索引
static int adc_filter_buffers[NUM_ADC_CHANNELS][FILTER_WINDOW_SIZE] = {0};
static int adc_filter_index[NUM_ADC_CHANNELS] = {0};
static int adc_filter_initialized = 0;

static esp_adc_cal_characteristics_t *adc_chars;

#define channel     ADC_CHANNEL_0               // ADC测量通道
#define width       ADC_WIDTH_BIT_12            // ADC分辨率
#define atten       ADC_ATTEN_DB_11             // ADC衰减
#define unit        ADC_UNIT_1                  // ADC1

// 中值滤波函数
static int median_filter(int channel_idx, int new_value) {
    // 初始化缓冲区（首次调用时）
    if (!adc_filter_initialized) {
        for (int i = 0; i < FILTER_WINDOW_SIZE; i++) {
            adc_filter_buffers[channel_idx][i] = new_value;
        }
        adc_filter_initialized = 1;
        return new_value;
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

static void app_joystick_task(void *arg)
{
    int read_raw_1=0, read_raw_2=0, read_raw_3=0;
    uint32_t voltage =0;
    float voltage_f = 0;
    adc1_config_width(width);// 12位分辨率

    //ADC_ATTEN_DB_0:表示参考电压为1.1V
    //ADC_ATTEN_DB_2_5:表示参考电压为1.5V
    //ADC_ATTEN_DB_6:表示参考电压为2.2V
    //ADC_ATTEN_DB_11:表示参考电压为3.3V
    //adc1_config_channel_atten( channel,atten);// 设置通道0和3.3V参考电压

    // 分配内存
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    // 对 ADC 特性进行初始化，使其能够正确地计算转换结果和补偿因素
    esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);

    while(1)
    {
        //采集三个通道的ADC值
        // 读取原始ADC值
        int raw_1 = adc1_get_raw(ADC1_CHANNEL_0);  //GPIO1
        int raw_2 = adc1_get_raw(ADC1_CHANNEL_1);  //gpio2

        // 应用中值滤波
        read_raw_1 = median_filter(0, raw_1);
        read_raw_2 = median_filter(1, raw_2);

        // 输出原始值和滤波后的值
        ESP_LOGI(TAG, "raw_1 = %d\tfiltered_1 = %d\traw_2 = %d\tfiltered_2 = %d",
                 raw_1, read_raw_1, raw_2, read_raw_2);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void joystick_task(void)
{
    // 增加栈大小到4KB以避免栈溢出
    xTaskCreate(app_joystick_task, "app_joystick_task", 4*1024, NULL, 4, NULL);
}