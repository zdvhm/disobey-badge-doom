# Main component - just app_main entry point
idf_component_register(
    SRCS "../prboom_main.c"
    INCLUDE_DIRS "."
    REQUIRES prboom prboom-esp32-compat esp_psram
)
