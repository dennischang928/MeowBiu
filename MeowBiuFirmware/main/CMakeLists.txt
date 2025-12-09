idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES display face lvgl service_manager rtc_service ui_service input_service network_service)