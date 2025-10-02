set(src_dirs 
                "."
                keyboard_led
                spi_scanner
                tinyusb_hid
                nvs_manager
                joystick
                ssd1306/oled_menu
                ssd1306/oled_fonts
                ssd1306/oled_driver
                ssd1306/oled_action
                "../hid_device"
                wifi_app
                init_manager)

set(include_dirs 
                "."
                keyboard_led
                spi_scanner
                tinyusb_hid
                nvs_manager
                init_manager
                joystick
                ssd1306/oled_menu
                ssd1306/oled_fonts
                ssd1306/oled_driver
                ssd1306/oled_action
                "../hid_device"
                wifi_app)

set(requires esp_timer
             driver
             freertos
             nvs_flash
             esp_http_server
             esp_wifi
             usb
             esp_adc
             )

# 定义需要嵌入到固件中的静态文件
set(embed_files
            ./wifi_app/index.html)

idf_component_register(SRC_DIRS ${src_dirs} 
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES ${requires}
                       EMBED_FILES ${embed_files}
                       )

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)

cmake_policy(SET CMP0079 NEW)
target_link_libraries(${tusb_lib} PRIVATE ${COMPONENT_LIB})
