# NVS Manager Component CMakeLists.txt

idf_component_register(
    SRCS "nvs_manager_base.cpp" "keymap_nvs_manager.cpp" "c_wrapper.cpp" "c_example_usage.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash
)

# 添加编译选项
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)
endif()