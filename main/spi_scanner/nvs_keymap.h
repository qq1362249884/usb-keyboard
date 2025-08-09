#ifndef _NVS_KEYMAP_H_
#define _NVS_KEYMAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>
#include "c_wrapper.h"
#include "spi_config.h"

// 外部声明运行时按键映射
extern uint16_t keymaps[2][NUM_KEYS];

// NVS相关函数声明
esp_err_t nvs_keymap_init(void);
esp_err_t save_keymap_to_nvs(uint8_t layer, const uint16_t *keymap);
esp_err_t load_keymap_from_nvs(uint8_t layer, uint16_t *keymap);
esp_err_t reset_keymap_to_default(uint8_t layer);
void test_keymap_config(void);
void nvs_keymap_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // _NVS_KEYMAP_H_