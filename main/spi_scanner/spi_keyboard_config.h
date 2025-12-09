#ifndef _SPI_KEYBOARD_CONFIG_H_
#define _SPI_KEYBOARD_CONFIG_H_

#include "keyboard_led/keyboard_led.h"

// ===============================
// SPI配置参数
// ===============================
#define SPI_HOST    SPI3_HOST   // 使用 SPI2 主机
#define PIN_NUM_QH   10         //74hc165 QH引脚
#define PIN_NUM_SCLK 11         //74hc165 CLK引脚
#define PIN_NUM_PL   12         //74hc165 PL引脚
#define NUM_KEYS     17         //按键数量
#define NUM_BYTES    3          //165数量

// ===============================
// 矩阵和按键映射配置
// ===============================
#define MATRIX_ROWS 5           // 矩阵行数
#define MATRIX_COLS 4           // 矩阵列数
#define KEY_INDEX_INVALID 255   // 按键索引无效值

// 行列坐标到按键索引的映射结构体
typedef struct {
    // 二维矩阵表示行列到按键索引的映射
    // 结构与keyboard_led.c中的g_led_config保持一致
    uint8_t matrix[MATRIX_ROWS][MATRIX_COLS];
    // 按键索引到行列坐标的映射表
    struct {
        uint8_t row;
        uint8_t col;
    } index_to_matrix[NUM_KEYS];
} key_mapping_config_t;

// 按键映射配置结构体
// 优化为与keyboard_led.c中的g_led_config相同的二维矩阵结构
static const key_mapping_config_t g_key_mapping_config = {
    {
        // Key Matrix to Key Index
        // 根据实际硬件连接顺序调整索引
        {0, 1, 2, 3},        // 第一行
        {4, 5, 6, 7},        // 第二行
        {8, 9, 10, KEY_INDEX_INVALID},  // 第三行 (第四个位置没有按键)
        {11, 12, 13, KEY_INDEX_INVALID}, // 第四行 (第四个位置没有按键)
        {14, KEY_INDEX_INVALID, 15, 16}   // 第五行 (第二个位置没有按键)
    },
    {
        // Key Index to Matrix Position
        {0, 0}, {0, 1}, {0, 2}, {0, 3},  // 按键索引0-3 → 第一行
        {1, 0}, {1, 1}, {1, 2}, {1, 3},  // 按键索引4-7 → 第二行
        {2, 0}, {2, 1}, {2, 2},          // 按键索引8-10 → 第三行
        {3, 0}, {3, 1}, {3, 2},          // 按键索引11-13 → 第四行
        {4, 0}, {4, 2}, {4, 3}           // 按键索引14-16 → 第五行
    }
};

/**
 * @brief 将按键索引转换为行列坐标
 * @param key_index 按键索引
 * @param row 输出参数，行坐标
 * @param col 输出参数，列坐标
 * @return 是否有对应的行列坐标
 */
static inline bool key_index_to_matrix(uint8_t key_index, uint8_t *row, uint8_t *col) {
    if (key_index >= NUM_KEYS) {
        return false;
    }
    
    *row = g_key_mapping_config.index_to_matrix[key_index].row;
    *col = g_key_mapping_config.index_to_matrix[key_index].col;
    
    // 检查是否有对应的LED
    return (g_led_config.matrix_co[*row][*col] != NO_LED);
}

/**
 * @brief 将行列坐标转换为按键索引
 * @param row 行坐标
 * @param col 列坐标
 * @return 按键索引，如果无效返回KEY_INDEX_INVALID
 */
static inline uint8_t matrix_to_key_index(uint8_t row, uint8_t col) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KEY_INDEX_INVALID;
    }
    
    return g_key_mapping_config.matrix[row][col];
}

#endif // _SPI_KEYBOARD_CONFIG_H_