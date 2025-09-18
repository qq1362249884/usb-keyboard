/*

 * OLED驱动头文件 [硬件驱动] 实现文件，移植时需要包含此文件

*/
#include "OLED_driver.h"

#define HARDWARE_I2C_SSD1306

#ifndef HARDWARE_I2C_SSD1306
#define OLED_SCL(x) do { x ? gpio_set_level(OLED_SCL_Pin, 1) : gpio_set_level(OLED_SCL_Pin, 0); } while (0)
#define OLED_SDA(x) do { x ? gpio_set_level(OLED_SDA_Pin, 1) : gpio_set_level(OLED_SDA_Pin, 0); } while (0)
#define OLED_SDA_READ() gpio_get_level(OLED_SDA_Pin)
#endif



uint8_t OLED_DisplayBuf[32 / 8][128]; // 显存
bool OLED_ColorMode = true;
i2c_master_dev_handle_t dev_handle;

#ifdef HARDWARE_I2C_SSD1306
uint8_t ack = 1;
void oled_ssd1306_init(void)
{
	i2c_master_bus_config_t i2c_bus_config = 
	{
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.i2c_port = -1,
		.scl_io_num = OLED_SCL_Pin,
		.sda_io_num = OLED_SDA_Pin,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

	i2c_device_config_t dev_cfg =
	{
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = OLED_ADDRESS,
		.scl_speed_hz = 100000, // 降低I2C速度到50kHz
	};
	ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

}

void OLED_Write_DATA(uint8_t data)
{
	uint8_t data_buf[2] = {0x40, data};
	i2c_master_transmit(dev_handle, data_buf, 2, -1);
}

void OLED_Write_CMD(uint8_t data)
{
	uint8_t data_buf[2] = {0x00, data};
	i2c_master_transmit(dev_handle, data_buf, 2, -1);
}

void OLED_WriteDataArr(uint8_t *Data, uint8_t Count)
{
	uint8_t data_buf[Count + 1];
	data_buf[0] = 0x40; // 数据模式
	memcpy(&data_buf[1], Data, Count);
	i2c_master_transmit(dev_handle, data_buf, Count + 1, -1);
}
#else
void oled_ssd1306_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << OLED_SDA_Pin | 1ULL << OLED_SCL_Pin,
		.mode = GPIO_MODE_OUTPUT_OD,
		.pull_down_en = 1,
		.pull_up_en = 1,
    };
    gpio_config(&io_conf);

	OLED_SCL(1);
	OLED_SDA(1);
}

void i2c_start(void)
{
	OLED_SDA(1);
	OLED_SCL(1);
	OLED_SDA(0);
	OLED_SCL(0);
}

void i2c_stop(void)
{
	OLED_SDA(0);
	OLED_SCL(1);
	OLED_SDA(1);
}

void i2c_write(uint8_t data)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_SDA(data & (0x80 >> i));
		esp_rom_delay_us(1);
		OLED_SCL(1);
		OLED_SCL(0);
	}
}

uint8_t i2c_receive_ack(void)
{
	uint8_t ack;
	OLED_SDA(1);
	OLED_SCL(1);
	esp_rom_delay_us(1);
	ack = OLED_SDA_READ();
	esp_rom_delay_us(1);
	OLED_SCL(0);
	return ack;
}



void OLED_Write_DATA(uint8_t data)
{
	i2c_start();
	i2c_write(0x78);
	i2c_receive_ack();
	i2c_write(0x40);
	i2c_receive_ack();
	i2c_write(data);
	i2c_receive_ack();
	i2c_stop();
}

void OLED_Write_CMD(uint8_t data)
{
	i2c_start();
	i2c_write(0x78);
	i2c_receive_ack();
	i2c_write(0x00);
	i2c_receive_ack();
	i2c_write(data);
	i2c_receive_ack();
	i2c_stop();
}

void OLED_WriteDataArr(uint8_t *Data, uint8_t Count)
{
	i2c_start();
	i2c_write(0x78);
	i2c_receive_ack();
	i2c_write(0x40);
	i2c_receive_ack();
	while (Count--)
	{
		i2c_write(*Data++);
		i2c_receive_ack();
	}
	i2c_stop();
}
#endif


// 颜色翻转函数
void OLED_ColorTurn(uint8_t i)
{
	if (i == 0)
	{
		OLED_Write_CMD(0xA6); // 正常显示
	}
	if (i == 1)
	{
		OLED_Write_CMD(0xA7); // 反色显示
	}
}

/**
 * 函数名：设置屏幕显示方向
 * 参数：Page 0=正常显示，1=屏幕内容翻转180度
 * 返回值：无
 * 说明：无
 */
void OLED_DisplayTurn(uint8_t i)
{
	// 先关闭屏幕，执行时可能会闪烁几次，所以先关闭，再打开
	OLED_Write_CMD(0xAE); // 关闭屏幕
	if (i == 0)
	{
		OLED_Write_CMD(0xC8); // 正常显示
		OLED_Write_CMD(0xA1);
	}
	if (i == 1)
	{
		OLED_Write_CMD(0xC0); // 翻转显示
		OLED_Write_CMD(0xA0);
	}
	OLED_Write_CMD(0xAF); // 打开屏幕
}

// 打开OLED显示
void OLED_DisPlay_On(void)
{
	OLED_Write_CMD(0x8D); // 电荷泵使能
	OLED_Write_CMD(0x14); // 开启电荷泵
	OLED_Write_CMD(0xAF); // 打开屏幕
}

// 关闭OLED显示
void OLED_DisPlay_Off(void)
{
	OLED_Write_CMD(0x8D); // 电荷泵使能
	OLED_Write_CMD(0x10); // 关闭电荷泵
	OLED_Write_CMD(0xAE); // 关闭屏幕
}
/**
 * 函数名：设置OLED光标显示位置
 * 参数：Page 指定光标所在的页，范围0~15
 * 参数：X 指定光标所在的X坐标，范围0~127
 * 返回值：无
 * 说明：OLED默认的Y轴，只有8Bit为一行写入，即1页有8个Y坐标
 */
void OLED_SetCursor(uint8_t Page, uint8_t X)
{
	/*如果使用此程序驱动1.3寸OLED显示，需要特别注意*/
	/*因为1.3寸OLED的驱动芯片是SH1106，有132列*/
	/*屏幕的起始列偏移了2列，所以我们的0列*/
	/*需要将X坐标加2，才能正常显示*/
#ifdef SH1106
	X += 2;
#endif

	/*通过指令设置页地址和列地址*/
	OLED_Write_CMD(0xB0 | Page);			  // 设置页位置
	OLED_Write_CMD(0x10 | ((X & 0xF0) >> 4)); // 设置X位置高4位
	OLED_Write_CMD(0x00 | (X & 0x0F));		  // 设置X位置低4位
}

// 更新显存到OLED
void OLED_Update(void)
{
	uint8_t j;
	/*遍历每一页 - 128x32分辨率只有4页(32/8)*/
	for (j = 0; j < 4; j++)
	{
		/*设置光标位置为每一页的第一列*/
		OLED_SetCursor(j, 0);
		/*连续写入128个数据，将显存数组中的数据写入到OLED硬件*/
		OLED_WriteDataArr(OLED_DisplayBuf[j], 128);
	}
}

/**
 * 函数名：更新OLED显存数组部分到OLED屏幕
 * 参数：X 指定区域左上角的横坐标，范围0~127
 * 参数：Y 指定区域左上角的纵坐标，范围0~127
 * 参数：Width 指定区域的宽度，范围0~128
 * 参数：Height 指定区域的高度，范围0~127
 * 返回值：无
 * 说明：此函数可以只更新指定区域的内容
 *         注意：Y坐标只能跨页，同一页内，剩余部分只能跨一页
 * 说明：所有的显示函数，都只是对OLED显存数组进行读写
 *         需要调用OLED_Update或者OLED_UpdateArea函数
 *         才会将显存数组中的数据发送到OLED硬件，从而显示出来
 *         实际显示效果需要刷新屏幕，所以需要调用更新函数
 */
void OLED_UpdateArea(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height)
{
	uint8_t j;

	/*边界检查，确保指定区域不会超出屏幕范围*/
	if (X > 128 - 1)
	{
		return;
	}
	if (Y > 64 - 1)
	{
		return;
	}
	if (X + Width > 128)
	{
		Width = 128 - X;
	}
	if (Y + Height > 64)
	{
		Height = 64 - Y;
	}

	/*计算指定区域涉及的页数*/
	/*(Y + Height - 1) / 8 + 1，目的(Y + Height) / 8，向上取整*/
	for (j = Y / 8; j < (Y + Height - 1) / 8 + 1; j++)
	{
		/*设置光标位置为该页的指定列*/
		OLED_SetCursor(j, X);
		/*连续写入Width个数据，将显存数组中的数据写入到OLED硬件*/
		OLED_WriteDataArr(&OLED_DisplayBuf[j][X], Width);
	}
}

extern void OLED_Clear(void);

// OLED的初始化
void OLED_Init(void)
{

	oled_ssd1306_init();
	
	OLED_Write_CMD(0xAE); 

	OLED_Write_CMD(0xD5); 
	OLED_Write_CMD(0x80); 

	OLED_Write_CMD(0xA8); 
	OLED_Write_CMD(0x1F); 

	OLED_Write_CMD(0xD3); 
	OLED_Write_CMD(0x00); 

	OLED_Write_CMD(0x40); 

	OLED_Write_CMD(0xA1); 
	OLED_Write_CMD(0xC8); 

	OLED_Write_CMD(0xDA); 
	OLED_Write_CMD(0x02); 

	OLED_Write_CMD(0x81); 
	OLED_Write_CMD(0xFF); 

	OLED_Write_CMD(0xD9); 
	OLED_Write_CMD(0xF1); 

	OLED_Write_CMD(0xDB); 
	OLED_Write_CMD(0x40); 

	OLED_Write_CMD(0xA4); 

	OLED_Write_CMD(0xA6); 

	OLED_Write_CMD(0x8D); 
	OLED_Write_CMD(0x14);

	OLED_Write_CMD(0xAF);


	OLED_Clear();
	OLED_Update();	
}

/**
 * 函数名：设置OLED亮度
 * 参数：Brightness 0-255，不同显示芯片效果可能不同
 * 返回值：无
 * 说明：需要设置亮度或亮度大小
 */
void OLED_Brightness(int16_t Brightness)
{

	// 检查亮度是否有变化，有变化时再发送指令
	static int16_t Last_Brightness;
	if (Brightness == Last_Brightness)
	{
		return;
	}
	else
	{
		Last_Brightness = Brightness;
	}

	if (Brightness > 255)
	{
		Brightness = 255;
	}
	if (Brightness < 0)
	{
		Brightness = 0;
	}
	OLED_Write_CMD(0x81);
	OLED_Write_CMD(Brightness);
}

/**
 * @brief 设置OLED显示模式
 * @param colormode true: 正常模式，false: 反色模式
 * @note OLED_ColorTurn 0: 正常模式，1: 反转模式
 * @return 无
 */
void OLED_SetColorMode(bool colormode)
{
	OLED_ColorMode = colormode;

	// 检查显示模式是否有变化，有变化时再发送指令
	static bool Last_OLED_ColorMode;
	if (OLED_ColorMode == Last_OLED_ColorMode)
	{
		return;
	}
	else
	{
		Last_OLED_ColorMode = OLED_ColorMode;
	}

	if (OLED_ColorMode)
		OLED_ColorTurn(0);
	else
		OLED_ColorTurn(1);
}
