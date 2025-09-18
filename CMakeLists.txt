# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.16)

# 引用ESP-IDF工具链
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义项目名称
project(usb-keyboard)

