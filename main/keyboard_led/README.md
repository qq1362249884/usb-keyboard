# 键盘LED灯效控制模块

这个模块提供了对键盘RGB LED灯效的完整控制，包括多种灯效模式、响应式交互和配置管理。

## 功能特性

- 支持43种灯效模式（通过keyboard_rgb_matrix库）
- 响应式灯效（按键时的视觉反馈）
- 完整的灯效控制接口（模式切换、HSV调节、速度控制等）
- 配置持久化存储（通过NVS保存用户偏好设置）
- 软件控光（避免频繁开关电源引脚）

## 支持的灯效列表

以下是keyboard_rgb_matrix库支持的43种灯效模式及其说明：

### 基础灯效
1. `RGB_MATRIX_SOLID_COLOR` - 单色显示
2. `RGB_MATRIX_GRADIENT_UP_DOWN` - 从上到下的渐变色
3. `RGB_MATRIX_GRADIENT_LEFT_RIGHT` - 从左到右的渐变色
4. `RGB_MATRIX_BREATHING` - 呼吸灯效果（亮度变化）
5. `RGB_MATRIX_BAND_SAT` - 饱和度变化的光带
6. `RGB_MATRIX_BAND_VAL` - 亮度变化的光带
7. `RGB_MATRIX_BAND_PINWHEEL_SAT` - 饱和度变化的风车光带
8. `RGB_MATRIX_BAND_PINWHEEL_VAL` - 亮度变化的风车光带
9. `RGB_MATRIX_BAND_SPIRAL_SAT` - 饱和度变化的螺旋光带
10. `RGB_MATRIX_BAND_SPIRAL_VAL` - 亮度变化的螺旋光带

### 彩虹灯效
11. `RGB_MATRIX_RAINBOW_MOVING_CHEVRON` - 移动的彩虹V型图案
12. `RGB_MATRIX_RAINBOW_PINWHEELS` - 彩虹风车效果
13. `RGB_MATRIX_RAINBOW_SNAKE` - 彩虹蛇形效果
14. `RGB_MATRIX_CYCLE_LEFT_RIGHT` - 彩虹左右循环
15. `RGB_MATRIX_CYCLE_UP_DOWN` - 彩虹上下循环
16. `RGB_MATRIX_CYCLE_OUT_IN` - 彩虹从外向内循环
17. `RGB_MATRIX_CYCLE_OUT_IN_DUAL` - 彩虹从两侧向中间循环
18. `RGB_MATRIX_RAINBOW_BEACON` - 彩虹灯塔效果
19. `RGB_MATRIX_RAINBOW_MOOD` - 彩虹心情灯（缓慢变化）
20. `RGB_MATRIX_RAINBOW_SWIRL` - 彩虹漩涡效果

### 响应式灯效
21. `RGB_MATRIX_TYPING_HEATMAP` - 打字热图（按键位置发热效果）
22. `RGB_MATRIX_DIGITAL_RAIN` - 数字雨效果
23. `RGB_MATRIX_SOLID_REACTIVE_SIMPLE` - 简单的按键反应效果
24. `RGB_MATRIX_SOLID_REACTIVE` - 按键反应效果
25. `RGB_MATRIX_SOLID_REACTIVE_WIDE` - 宽范围按键反应效果
26. `RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE` - 多范围按键反应效果
27. `RGB_MATRIX_SOLID_REACTIVE_CROSS` - 十字形按键反应效果
28. `RGB_MATRIX_SOLID_REACTIVE_MULTICROSS` - 多十字形按键反应效果
29. `RGB_MATRIX_SOLID_REACTIVE_NEXUS` - 中心点按键反应效果
30. `RGB_MATRIX_SOLID_REACTIVE_MULTINEXUS` - 多中心点按键反应效果
31. `RGB_MATRIX_SPLASH` - 按键水波纹效果
32. `RGB_MATRIX_MULTISPLASH` - 多按键水波纹效果
33. `RGB_MATRIX_SOLID_SPLASH` - 单色水波纹效果
34. `RGB_MATRIX_SOLID_MULTISPLASH` - 单色多水波纹效果

### 特殊效果
35. `RGB_MATRIX_EFFECT_CHRISTMAS` - 圣诞主题灯效
36. `RGB_MATRIX_EFFECT_KNIGHT` - 骑士巡逻效果
37. `RGB_MATRIX_EFFECT_ALTERNATING` - 交替闪烁效果
38. `RGB_MATRIX_EFFECT_TWINKLE` - 闪烁星星效果
39. `RGB_MATRIX_EFFECT_SOLID_COLOR` - 单色填充效果
40. `RGB_MATRIX_EFFECT_SOLID_REACTIVE_SIMPLE` - 简单按键响应
41. `RGB_MATRIX_EFFECT_SOLID_REACTIVE` - 按键响应效果
42. `RGB_MATRIX_EFFECT_SOLID_REACTIVE_WIDE` - 宽范围按键响应
43. `RGB_MATRIX_EFFECT_SOLID_REACTIVE_MULTIWIDE` - 多范围按键响应

## 接口说明

### 初始化函数

```c
// 初始化WS2812B LED驱动
esp_err_t kob_ws2812b_init(led_strip_handle_t *led_strip);

// 初始化RGB矩阵
esp_err_t kob_rgb_matrix_init(void);

// 创建LED任务
void led_task(void);
```

### 灯效控制函数

```c
// 设置灯效模式
esp_err_t kob_rgb_matrix_set_mode(uint16_t mode);

// 设置HSV颜色值
esp_err_t kob_rgb_matrix_set_hsv(uint8_t hue, uint8_t sat, uint8_t val);

// 设置灯效速度
esp_err_t kob_rgb_matrix_set_speed(uint8_t speed);

// 切换到下一个灯效模式
esp_err_t kob_rgb_matrix_next_mode(void);

// 切换到上一个灯效模式
esp_err_t kob_rgb_matrix_prev_mode(void);
```

### 电源控制函数

```c
// 开启/关闭LED灯
esp_err_t kob_ws2812_enable(bool enable);

// 清除所有LED灯
esp_err_t kob_ws2812_clear(void);

// 检查LED灯是否开启
bool kob_ws2812_is_enable(void);
```

### 配置管理函数

```c
// 保存配置到NVS
esp_err_t kob_rgb_save_config(void);

// 从NVS加载配置
esp_err_t kob_rgb_load_config(void);

// 获取配置结构体指针
led_effect_config_t* kob_rgb_get_config(void);
```

### 键盘响应处理函数

```c
// 处理键盘事件（通常由spi_scanner调用）
void kob_rgb_process_key_event(uint8_t row, uint8_t col, bool pressed);
```

## 使用示例

### 基本使用

```c
// 初始化LED任务
led_task();

// 开启LED灯
kob_ws2812_enable(true);

// 设置灯效模式为彩虹循环
kob_rgb_matrix_set_mode(RGB_MATRIX_RAINBOW_MOVING_CHEVRON);

// 设置颜色为蓝色
kob_rgb_matrix_set_hsv(160, 255, 128);

// 增加灯效速度
rgb_matrix_increase_speed();
```

### 配置管理

```c
// 加载之前保存的配置
kob_rgb_load_config();

// 修改设置后保存
kob_rgb_matrix_set_mode(RGB_MATRIX_CYCLE_LEFT_RIGHT);
kob_rgb_save_config();
```

## 注意事项

1. 所有灯效模式在使用前需要确保RGB_MATRIX_EFFECT_MAX宏已正确定义
2. 软件控光模式下（KOB_WS2812_USE_SOFTWARE_POWER_OFF=1），关闭LED实际上是将所有灯珠亮度设置为0
3. 配置保存会自动在修改设置后执行，也可以手动调用kob_rgb_save_config()
4. 响应式灯效需要确保RGB_MATRIX_KEYREACTIVE_ENABLED已定义

## 灯效开启方法

### 1. 通过代码直接设置灯效

您可以在代码中直接调用`kob_rgb_matrix_set_mode()`函数设置特定的灯效模式：

```c
// 设置灯效模式为彩虹移动V型图案
kob_rgb_matrix_set_mode(RGB_MATRIX_RAINBOW_MOVING_CHEVRON);

// 设置灯效模式为数字雨
kob_rgb_matrix_set_mode(RGB_MATRIX_DIGITAL_RAIN);

// 设置灯效模式为打字热图
kob_rgb_matrix_set_mode(RGB_MATRIX_TYPING_HEATMAP);
```

### 2. 使用模式切换函数

您可以使用以下函数在不同灯效之间切换：

```c
// 切换到下一个灯效模式
kob_rgb_matrix_next_mode();

// 切换到上一个灯效模式
kob_rgb_matrix_prev_mode();
```

### 3. 通过配置文件启用灯效

灯效的启用状态可以在managed_components/lijunru-hub__keyboard_rgb_matrix/Kconfig文件中配置。您可以使用ESP-IDF的menuconfig工具修改这些设置：

```bash
idf.py menuconfig
```

然后导航到以下路径：
`Component config` > `keyboard_rgb_matrix` > `RGB Matrix Effects`

在这里，您可以选择启用或禁用特定的灯效。每个灯效都有一个对应的`CONFIG_ENABLE_RGB_MATRIX_XXX`选项。

### 4. 灯效参数调节

您可以调节灯效的各种参数，以获得不同的视觉效果：

```c
// 设置HSV颜色值（色调、饱和度、亮度）
kob_rgb_matrix_set_hsv(0, 255, 128); // 红色，最大饱和度，中等亮度

// 设置灯效速度
kob_rgb_matrix_set_speed(128); // 中等速度

// 增加灯效速度
rgb_matrix_increase_speed();

// 减小灯效速度
rgb_matrix_decrease_speed();
```

## 已启用的灯效详细说明

通过menuconfig配置，以下灯效已被启用：

### 响应式灯效

1. **打字热图 (Typing Heatmap)**
   - 功能：按键位置会显示发热效果，根据按键频率和时间产生热图效果
   - 特点：直观反馈按键使用情况，视觉效果生动
   - 触发方式：按键时自动触发
   - 对应常量：`RGB_MATRIX_TYPING_HEATMAP`

2. **数字雨 (Digital Rain)**
   - 功能：类似电影《黑客帝国》中的数字雨效果
   - 特点：字符从屏幕顶部飘落，营造科技感
   - 触发方式：持续显示，按键时可能产生额外效果
   - 对应常量：`RGB_MATRIX_DIGITAL_RAIN`

### 彩虹灯效

3. **彩虹灯塔 (Rainbow Beacon)**
   - 功能：彩虹色光效从中心向外扩散，如同灯塔旋转
   - 特点：色彩鲜艳，动态效果明显
   - 触发方式：持续循环显示
   - 对应常量：`RGB_MATRIX_RAINBOW_BEACON`

4. **彩虹移动V型 (Rainbow Moving Chevron)**
   - 功能：V字形彩虹图案在键盘上移动
   - 特点：动感强，视觉上有流动感
   - 触发方式：持续循环移动
   - 对应常量：`RGB_MATRIX_RAINBOW_MOVING_CHEVRON`

5. **彩虹风车 (Rainbow Pinwheels)**
   - 功能：多个彩色风车图案在键盘上旋转
   - 特点：复杂而美丽的几何动态效果
   - 触发方式：持续旋转显示
   - 对应常量：`RGB_MATRIX_RAINBOW_PINWHEELS`

### 其他特效

6. **河流流动 (Riverflow)**
   - 功能：模拟河流流动的光效
   - 特点：平滑的色彩过渡，如同水流
   - 触发方式：持续流动显示
   - 对应常量：`RGB_MATRIX_RIVERFLOW`

7. **星光双色调 (Starlight Dual Hue)**
   - 功能：两种色调的星光效果
   - 特点：柔和的闪烁效果，营造星空感
   - 触发方式：持续闪烁显示
   - 对应常量：`RGB_MATRIX_STARLIGHT_DUAL_HUE`

8. **星光双饱和度 (Starlight Dual Saturation)**
   - 功能：不同饱和度的星光效果
   - 特点：亮度变化的闪烁效果，更加自然
   - 触发方式：持续闪烁显示
   - 对应常量：`RGB_MATRIX_STARLIGHT_DUAL_SATURATION`

## 灯效配置方法详解

### 通过menuconfig配置灯效

1. **打开menuconfig配置界面**
   ```bash
   source ~/esp/esp-idf/export.sh && idf.py menuconfig
   ```

2. **导航至RGB灯效配置菜单**
   - 路径：`Component config → rgb matrix → RGB Matrix Effects`
   - 在这个菜单中可以看到所有可用的灯效选项

3. **启用/禁用灯效**
   - 使用空格键选择或取消选择灯效选项
   - 已启用的灯效前面会显示 `[*]` 标记
   - 未启用的灯效前面显示 `[ ]` 标记

4. **保存配置**
   - 完成选择后，按 `S` 键保存配置
   - 按 `Q` 键退出配置界面

5. **重新编译烧录**
   - 保存配置后，需要重新编译并烧录固件，新的灯效配置才能生效
   ```bash
   idf.py build flash monitor
   ```

### 灯效控制接口详解

键盘程序中提供了以下控制灯效的接口：

1. **模式切换**：可以通过特定按键组合切换不同的灯效模式
   ```c
   // 设置灯效模式
   kob_rgb_matrix_set_mode(uint16_t mode);
   
   // 切换到下一个灯效模式
   kob_rgb_matrix_next_mode();
   
   // 切换到上一个灯效模式
   kob_rgb_matrix_prev_mode();
   ```

2. **HSV调节**：可以调整灯效的色相(Hue)、饱和度(Saturation)和亮度(Value)
   ```c
   // 设置HSV颜色值
   kob_rgb_matrix_set_hsv(uint8_t hue, uint8_t sat, uint8_t val);
   ```

3. **速度控制**：可以调整灯效动画的速度
   ```c
   // 设置灯效速度
   kob_rgb_matrix_set_speed(uint8_t speed);
   
   // 增加灯效速度
   rgb_matrix_increase_speed();
   
   // 减小灯效速度
   rgb_matrix_decrease_speed();
   ```

4. **方向控制**：某些灯效可以调整运动方向

## 常见问题

**Q: 灯效不响应键盘操作怎么办？**
A: 通过检查spi_scanner.c文件，确认该文件中已经在build_hid_report函数内正确调用了kob_rgb_process_key_event函数来处理按键事件。如果灯效仍然不响应，请检查：
1. LED初始化是否成功完成
2. RGB矩阵相关配置是否正确
3. 当前使用的灯效模式是否支持键盘响应功能

**Q: 如何添加自定义灯效？**
A: 参考keyboard_rgb_matrix库的文档，创建自定义灯效函数并在rgb_matrix_user.inc中注册

**Q: 保存的配置在重启后丢失怎么办？**
A: 确保正确调用了kob_rgb_save_config()，并检查NVS存储空间是否足够