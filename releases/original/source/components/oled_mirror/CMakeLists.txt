# OLED Mirror — ESP-IDF 独立组件
# 通过 USB CDC 将 U8g2 帧缓冲透传到 PC，设备端零处理
# 编译开关由根 CMake 的 BLUE_LONG_ENABLE_MIRROR 统一注入，组件不另设默认值。
idf_component_register(
    SRCS "oled_mirror.c"
    INCLUDE_DIRS "."
    REQUIRES u8g2 esp_timer
)
