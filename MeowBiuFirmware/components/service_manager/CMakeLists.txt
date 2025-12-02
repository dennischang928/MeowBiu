# Register the UI service component
idf_component_register(
    # Source files to compile
    SRCS 
        "service_manager.c"
        "event_bus.c"
        # "ui_screens.c"  # Add if you split screen logic
    # Public include directories (visible to other components)
    INCLUDE_DIRS 
        "include"
    
    # Private include directories (only visible to this component)
    # PRIV_INCLUDE_DIRS 
    #     "internal"
    
    # Components this component REQUIRES (public dependencies)
    # These are exposed to users of ui_service
    REQUIRES
        esp_event      # Event library for event handling
    
    # Components this component privately requires (private dependencies)
    # These are NOT exposed to users of ui_service
    PRIV_REQUIRES
        freertos       # FreeRTOS is implementation detail (usually auto-included)
)

# Optional: Add compile options for this component only
# target_compile_options(${COMPONENT_LIB} PRIVATE
#     -Wall
#     -Wextra
#     -Werror
# )

# Optional: Add preprocessor definitions
# target_compile_definitions(${COMPONENT_LIB} PRIVATE
#     UI_DEBUG=1
# )