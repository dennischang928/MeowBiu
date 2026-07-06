idf_component_register(SRCS "clock_ui.c"
                            "clock_faces/face_registry/face_registry.c"
                            "clock_faces/face_old_fashioned/face_old_fashioned.c"
                            "clock_faces/face_digital/face_digital.c"
                            "clock_faces/face_simple_weather/face_simple_weather.c"
                    INCLUDE_DIRS "include"
                    REQUIRES lvgl network_service)
