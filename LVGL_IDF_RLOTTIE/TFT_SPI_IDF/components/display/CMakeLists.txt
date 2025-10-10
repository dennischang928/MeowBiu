idf_component_register(
    SRCS "display.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd esp_lcd_gc9a01 lvgl esp_timer
)

set(LOTTIE_MODULE OFF)
set(LOTTIE_THREAD OFF)
set(BUILD_SHARED_LIBS OFF)