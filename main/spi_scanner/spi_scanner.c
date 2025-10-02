#include "spi_scanner.h"
#include "tinyusb_hid.h" // 添加tinyusb_hid头文件，用于访问tud_suspended()和tud_remote_wakeup()函数
#include "ssd1306/oled_menu/oled_menu_display.h"
#include "keyboard_led/keyboard_led.h"
#include "spi_keyboard_config.h" // 合并后的SPI和按键映射配置文件
#include "keymap_manager.h" // 添加组合键支持

// 外部声明当前映射层变量
extern uint8_t current_keymap_layer;

spi_device_handle_t spi_device = NULL; // SPI句柄
uint8_t received_data[NUM_BYTES];
uint8_t debounce_data[NUM_BYTES]; 
uint16_t remap_data[NUM_KEYS];


static void spi_hid_init(void)
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

static void read_74hc165_data() {
    spi_transaction_t transaction = {
        .length = NUM_BYTES * 8,           // 总共读取多少 bit
        .rx_buffer = received_data,
    };

    gpio_set_level(PIN_NUM_PL, 0); // 拉低，开始加载
    esp_rom_delay_us(10);                    // 短暂延时确保加载完成
    gpio_set_level(PIN_NUM_PL, 1); // 拉高，进入移位模式 
    spi_device_transmit(spi_device, &transaction); // 发起SPI传输
}

static void apply_debounce_filter(uint16_t filter_timeus)
{
    uint8_t a;
    memcpy(debounce_data, received_data, NUM_BYTES);

    esp_rom_delay_us(filter_timeus); // 等待滤波时间
    read_74hc165_data();
    for(uint16_t i = 0; i < NUM_BYTES; i++)
    {
        a = debounce_data[i] ^ received_data[i]; // 按键状态变化
        received_data[i] |= a; // 更新滤波后的数据
    }
}

static hid_report_t build_hid_report(uint8_t _layer)
{
    //局部变量和结构体初始化
    uint8_t modify = 0;
    uint8_t keynum = 0;
    const uint8_t full_bytes = NUM_KEYS / 8; // 完整字节数
    const uint8_t remaining_bits = NUM_KEYS % 8; // 剩余位数
    uint8_t bit_index = 0;

    // 保存当前按键状态用于下一次比较
    static uint8_t prev_key_state[NUM_BYTES] = {0};

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
            uint16_t key_index = i * 8 + j;
            bool current_state = (received_data[i] & (0x80 >> j)) != 0;
            bool prev_state = (prev_key_state[i] & (0x80 >> j)) != 0;
            
            // 检测按键状态变化
            if (current_state != prev_state) {
                // 使用按键映射表将按键索引转换为行列坐标
                uint8_t row, col;
                if (key_index_to_matrix(key_index, &row, &col)) {
                    // 只有当按键有对应的LED时才处理RGB矩阵按键事件
                    kob_rgb_process_key_event(row, col, current_state);
                }
            }
            
            if (current_state) {
                keymap->key_pressed_data[keymap->key_pressed_num++] = key_index;
                //ESP_LOGI("usb_spi", "key_pressed_data: %d", key_index);
            } else {
                keymap->key_release_num++;
            }
        }
        
        // 保存当前状态用于下一次比较
        prev_key_state[i] = received_data[i];
    }

    for (uint16_t i = 0; i < keymap->key_pressed_num; i++) {
        uint8_t key = keymap->key_pressed_data[i];
        uint16_t kc = keymaps[_layer][key];

        // 将按键代码发送到键盘队列
        QueueHandle_t keyboard_queue = get_keyboard_queue();
        if (keyboard_queue != NULL) {
            xQueueSend(keyboard_queue, &kc, 0);
        }

        // 检查是否为组合键
        if (is_combo_key(kc)) {
            // 组合键处理：提取修饰键和基础键
            uint16_t modifier_mask = get_modifier_mask(kc);
            uint16_t base_key = get_base_key(kc);
            
            // 处理修饰键（简化方案：只使用左修饰键）
            if (modifier_mask & MOD_LCTRL) modify |= (1 << 0);  // KC_LEFT_CTRL
            if (modifier_mask & MOD_LSHIFT) modify |= (1 << 1); // KC_LEFT_SHIFT
            if (modifier_mask & MOD_LALT) modify |= (1 << 2);    // KC_LEFT_ALT
            if (modifier_mask & MOD_LGUI) modify |= (1 << 3);    // KC_LEFT_GUI
            // 删除右修饰键处理，因为左修饰键和右修饰键功能相同
            // if (modifier_mask & MOD_RCTRL) modify |= (1 << 4);   // KC_RIGHT_CTRL
            // if (modifier_mask & MOD_RSHIFT) modify |= (1 << 5);  // KC_RIGHT_SHIFT
            // if (modifier_mask & MOD_RALT) modify |= (1 << 6);    // KC_RIGHT_ALT
            // if (modifier_mask & MOD_RGUI) modify |= (1 << 7);    // KC_RIGHT_GUI
            
            // 处理基础键
            if (base_key != KC_NO) {
                remap_data[keynum++] = base_key;
            }
        } else {
            // 普通按键处理
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

/**
 * @brief 当需要时唤醒USB主机
 * 
 * 检查主机是否允许远程唤醒，并在USB挂起时执行唤醒操作
 */
extern bool s_remote_wakeup_enabled; // 从tinyusb_hid.c引用远程唤醒允许标志

static void wakeup_host_if_needed(void)
{
    // 检查主机是否允许远程唤醒
    if (!s_remote_wakeup_enabled) {
        ESP_LOGD("usb_spi", "Remote wakeup not enabled by host");
        return;
    }
    
    // 检查USB是否处于挂起状态
    if (tud_suspended()) {
        // 唤醒主机
        ESP_LOGI("usb_spi", "Waking up host from suspend mode");
        if (tud_remote_wakeup()) {
            ESP_LOGI("usb_spi", "Remote wakeup signal sent successfully");
        } else {
            ESP_LOGW("usb_spi", "Failed to send remote wakeup signal");
        }
    }
}

static void spi_scanner_task(void *pvParameter)
 {
    spi_hid_init();
    tinyusb_hid_init();
    nvs_keymap_init(); // 初始化NVS并加载按键映射

    // 初始化上一次按键状态
    uint8_t prev_received_data[NUM_BYTES] = {0};
    bool keys_pressed = false;
    bool key_state_changed = false;

    while(1)
    {
        read_74hc165_data();
        apply_debounce_filter(10); // 减少去抖动延迟到10ms

        // 检测按键状态变化
        keys_pressed = false;
        key_state_changed = false;
        
        for(int i = 0; i < NUM_BYTES; i++)
        {
            if(received_data[i] != 0) // 有按键被按下
            {
                keys_pressed = true;
            }
            
            // 检测按键状态是否发生变化
            if(received_data[i] != prev_received_data[i])
            {
                key_state_changed = true;
            }
        }

        // 只有当按键状态从释放变为按下时才尝试唤醒主机
        // 这样可以避免电脑刚进入睡眠状态就被唤醒的问题
        if(keys_pressed && key_state_changed)
        {
            // 增加一个小延时，确保电脑已经完全进入睡眠状态
            vTaskDelay(10 / portTICK_PERIOD_MS); // 减少唤醒延迟到10ms
            wakeup_host_if_needed();
        }

        build_hid_report(current_keymap_layer); // 使用当前选择的映射层
        
        // 保存当前按键状态用于下一次比较
        memcpy(prev_received_data, received_data, NUM_BYTES);
        
        vTaskDelay(5 / portTICK_PERIOD_MS); // 减少扫描延迟到5ms                
    }
    vTaskDelete(NULL);
}

void spi_scanner_keyboard_task(void)
{
    xTaskCreate(spi_scanner_task, "spi_scanner_task", 4096, NULL, 5, NULL);
}

