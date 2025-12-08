# Register the RTC service component
idf_component_register(
    # Source files to compile
    SRCS 
        "rtc_service.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        # Nothing! We keep the header clean
    PRIV_REQUIRES
        service_manager
        esp_timer
        driver          # For I2C driver
        freertos
)
