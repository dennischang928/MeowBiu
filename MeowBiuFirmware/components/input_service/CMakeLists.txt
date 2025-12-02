# Register the UI service component
idf_component_register(
    # Source files to compile
    SRCS 
        "input_service.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        driver         # For hardware drivers
        # Nothing! We keep the header clean
    PRIV_REQUIRES
        service_manager
)

