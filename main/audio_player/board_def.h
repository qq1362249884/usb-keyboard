/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _AUDIO_BOARD_DEFINITION_H_
#define _AUDIO_BOARD_DEFINITION_H_

/**
 * @brief I2S 扬声器接口引脚配置
 * 
 * 扬声器通过 I2S 接口与 ESP32-S3 通信，用于音频输出
 */
#define I2S_SPK_LRC_PIN           GPIO_NUM_6   /**< 扬声器LR时钟引脚 (Left/Right Clock) - 控制左右声道切换 */
#define I2S_SPK_BCLK_PIN          GPIO_NUM_5   /**< 扬声器位时钟引脚 (Bit Clock) - 控制数据传输时序 */
#define I2S_SPK_DIN_PIN           GPIO_NUM_4    /**< 扬声器数据输入引脚 (Data Input) - 接收音频数据 */

#define BUTTON_VOLUP_ID           0     /* You need to define the GPIO pins of your board */
#define BUTTON_VOLDOWN_ID         1     /* You need to define the GPIO pins of your board */
#define BUTTON_MUTE_ID            2     /* You need to define the GPIO pins of your board */
#define BUTTON_SET_ID             3     /* You need to define the GPIO pins of your board */
#define BUTTON_MODE_ID            4     /* You need to define the GPIO pins of your board */
#define BUTTON_PLAY_ID            5     /* You need to define the GPIO pins of your board */
#define PA_ENABLE_GPIO            6     /* You need to define the GPIO pins of your board */
#define ADC_DETECT_GPIO           7     /* You need to define the GPIO pins of your board */
#define BATTERY_DETECT_GPIO       8     /* You need to define the GPIO pins of your board */
#define SDCARD_INTR_GPIO          9     /* You need to define the GPIO pins of your board */

#define SDCARD_OPEN_FILE_NUM_MAX  5

#define BOARD_PA_GAIN             (10) /* Power amplifier gain defined by board (dB) */

#define SDCARD_PWR_CTRL             -1
#define ESP_SD_PIN_CLK              -1
#define ESP_SD_PIN_CMD              -1
#define ESP_SD_PIN_D0               -1
#define ESP_SD_PIN_D1               -1
#define ESP_SD_PIN_D2               -1
#define ESP_SD_PIN_D3               -1
#define ESP_SD_PIN_D4               -1
#define ESP_SD_PIN_D5               -1
#define ESP_SD_PIN_D6               -1
#define ESP_SD_PIN_D7               -1
#define ESP_SD_PIN_CD               -1
#define ESP_SD_PIN_WP               -1

// MAX98357A不需要音频编解码器配置，移除相关定义

#define INPUT_KEY_NUM     4             /* You need to define the number of input buttons on your board */

#define INPUT_KEY_DEFAULT_INFO() {                      \
    {                                                   \
        .type = PERIPH_ID_ADC_BTN,                      \
        .user_id = INPUT_KEY_USER_ID_VOLUP,             \
        .act_id = BUTTON_VOLUP_ID,                      \
    },                                                  \
    {                                                   \
        .type = PERIPH_ID_ADC_BTN,                      \
        .user_id = INPUT_KEY_USER_ID_VOLDOWN,           \
        .act_id = BUTTON_VOLDOWN_ID,                    \
    },                                                  \
    {                                                   \
        .type = PERIPH_ID_ADC_BTN,                      \
        .user_id = INPUT_KEY_USER_ID_MUTE,              \
        .act_id = BUTTON_MUTE_ID,                       \
    },                                                  \
    {                                                   \
        .type = PERIPH_ID_ADC_BTN,                      \
        .user_id = INPUT_KEY_USER_ID_SET,               \
        .act_id = BUTTON_SET_ID,                        \
    },                                                  \
}

#endif
