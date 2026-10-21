# 以下五行样板代码必须按此顺序放置在项目的 CMakeLists 中，
# 否则 cmake 无法正常工作
cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "1.0.0-noautosleep")
option(BLUE_LONG_ENABLE_MIRROR "USB OLED mirror and local input control" ON)
if(BLUE_LONG_ENABLE_MIRROR)
    add_compile_definitions(ENABLE_OLED_MIRROR=1)
else()
    add_compile_definitions(ENABLE_OLED_MIRROR=0)
endif()

# 将 firmware/ 和 components/ 注册为自定义组件目录
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/firmware"
                         "${CMAKE_CURRENT_SOURCE_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include("$ENV{IDF_PATH}/tools/cmake/version.cmake")
if(NOT "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}" STREQUAL "5.5.4")
    message(FATAL_ERROR "BLUE_LONG_MESH requires ESP-IDF v5.5.4; activate that toolchain first.")
endif()
project(BLUE_LONG_MESH)

# Mirror is enabled by default; use -DBLUE_LONG_ENABLE_MIRROR=OFF to disable it.
