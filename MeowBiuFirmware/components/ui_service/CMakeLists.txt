# Register the UI service component
idf_component_register(
    # Source files to compile
    SRCS 
        "ui_service.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        # Nothing! We keep the header clean
    PRIV_REQUIRES
        lvgl           # LVGL is implementation detail
        face           # Face service is implementation detail
        service_manager
        clock_ui
)

