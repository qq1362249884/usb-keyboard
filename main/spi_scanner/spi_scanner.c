#include "spi_scanner.h"


spi_device_handle_t spi_device = NULL; // SPI句柄
uint8_t received_data[NUM_BYTES];
uint8_t debounce_data[NUM_BYTES]; 
uint16_t remap_data[NUM_KEYS];


void spi_hid_init(void)
{
    // 初始化 SH/LD 引脚
    gpio_set_direction(PIN_NUM_PL, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_PL, 1); // 默认拉高

    esp_err_t ret;

    // 配置 SPI 总线
    spi_bus_config_t buscfg = {
        .mosi_io_num = -1,               // 不使用 MOSI
        .miso_io_num = PIN_NUM_QH,       // MISO -> QH
        .sclk_io_num = PIN_NUM_SCLK,     // CLK
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = NUM_BYTES,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,            //Clock out at 1MHz
        .mode = 2,                            //SPI mode 0
        .spics_io_num = -1,                   //CS pin
        .queue_size = 1,                      //We want to be able to queue 7 transactions at a time
    };
    ret = spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    ret = spi_bus_add_device(SPI_HOST, &devcfg, &spi_device);
    ESP_ERROR_CHECK(ret);

    ESP_LOGI("usb_spi", "spi init success");

}

void read_74hc165_data() {
    spi_transaction_t transaction = {
        .length = NUM_BYTES * 8,           // 总共读取多少 bit
        .rx_buffer = received_data,
    };

    gpio_set_level(PIN_NUM_PL, 0); // 拉低，开始加载
    usleep(10);                    // 短暂延时确保加载完成
    gpio_set_level(PIN_NUM_PL, 1); // 拉高，进入移位模式 
    spi_device_transmit(spi_device, &transaction); // 发起SPI传输
}

void apply_debounce_filter(uint16_t filter_timeus)
{
    uint8_t a;
    memcpy(debounce_data, received_data, NUM_BYTES);

    usleep(filter_timeus); // 等待滤波时间
    read_74hc165_data();
    for(uint16_t i = 0; i < NUM_BYTES; i++)
    {
        a = debounce_data[i] ^ received_data[i]; // 按键状态变化
        received_data[i] |= a; // 更新滤波后的数据
    }
}

hid_report_t build_hid_report(uint8_t _layer)
{
    //局部变量和结构体初始化
    uint8_t modify = 0;
    uint8_t keynum = 0;
    const uint8_t full_bytes = NUM_KEYS / 8; // 完整字节数
    const uint8_t remaining_bits = NUM_KEYS % 8; // 剩余位数
    uint8_t bit_index = 0;

    keymap_t *keymap = malloc(sizeof(keymap_t) + NUM_KEYS * sizeof(uint16_t));
    hid_report_t kbd_hid_report = {0};

    if (!keymap) {
        // 内存分配失败处理, 返回一个空的报告
        return kbd_hid_report;
    } 
    memset(keymap, 0, sizeof(keymap_t) + NUM_KEYS * sizeof(uint16_t));
      
    for (uint16_t i = 0; i < NUM_BYTES; i++) {
        // 计算当前字节有效位数
        bit_index = (i < full_bytes) ? 8 : remaining_bits;
        for(uint16_t j = 0; j < bit_index; j++) { 
            if (received_data[i] & (0x80 >> j)) {
                keymap->key_pressed_data[keymap->key_pressed_num++] = i * 8 + j;
                //ESP_LOGI("usb_spi", "key_pressed_data: %d", i * 8 + j);
            } else {
                keymap->key_release_num++;
            }
        }
    }

    for (uint16_t i = 0; i < keymap->key_pressed_num; i++) {
        uint8_t key = keymap->key_pressed_data[i];
        uint16_t kc = keymaps[_layer][key];

        switch (kc){
        //修饰键处理，若有修饰键按下，则将修饰键状态更新到结构体中
        case KC_LEFT_CTRL ... KC_RIGHT_GUI:
            // Modifier key
            modify |= 1 << (kc - KC_LEFT_CTRL); // Corrected the base for modifier calculation
            continue;
            break; 
        //普通按键处理
        default:
            if (kc != KC_NO) {
                remap_data[keynum++] = kc;
            }
            break;                   
        }
    }

    if (keynum <= 6) {
        kbd_hid_report.report_id = REPORT_ID_KEYBOARD;
        kbd_hid_report.keyboard_report.modifier = modify;
        for (int i = 0; i < keynum; i++) {
            kbd_hid_report.keyboard_report.keycode[i] = remap_data[i];
            //ESP_LOGI("usb_spi", "key_pressed_report: %d", kbd_hid_report.keyboard_report.keycode[i]);
        }
    } else {
        kbd_hid_report.report_id = REPORT_ID_FULL_KEY_KEYBOARD;
        kbd_hid_report.keyboard_full_key_report.modifier = modify;
        for (int i = 0; i < keynum; i++) {
            // USAGE ID for keyboard starts from 4
            uint8_t key = remap_data[i] - 3;
            uint8_t byteIndex = (key - 1) / 8;
            uint8_t bitIndex = (key - 1) % 8;
            kbd_hid_report.keyboard_full_key_report.keycode[byteIndex] |= (1 << bitIndex);
        }
    }
    
    tinyusb_hid_keyboard_report(kbd_hid_report);


    free(keymap);

    return kbd_hid_report;
}

static void spi_scanner_task(void *pvParameter)
 {

    tinyusb_hid_init();
    nvs_keymap_init(); // 初始化NVS并加载按键映射

    while(1)
    {
        read_74hc165_data();
        apply_debounce_filter(150); 
        build_hid_report(1); // 使用层1的映射
        vTaskDelay(20 / portTICK_PERIOD_MS);                
    }
    vTaskDelete(NULL);
}

void spi_scanner_keyboard_task(void)
{
    xTaskCreate(spi_scanner_task, "spi_scanner_task", 4096, NULL, 5, NULL);
}

