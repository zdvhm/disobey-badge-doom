# PrBoom ESP32 Compatibility Layer Component
# Adapted for Disobey Badge 2025/2026 (ESP32-S3, no WiFi streaming)
idf_component_register(
    SRCS
        "i_main.c"
        "i_system.c"
        "i_video.c"
        "i_sound.c"
        "i_network.c"
        "spi_lcd.c"
        "gamepad.c"
        "psxcontroller.c"
    INCLUDE_DIRS "include" "."
    REQUIRES
        esp_timer
        esp_partition
        fatfs
        driver
        esp_psram
        spi_flash
        vfs
    PRIV_REQUIRES
        prboom
)

# Compiler options - suppress warnings for legacy code
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Os
    -DHAVE_CONFIG_H
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-discarded-qualifiers
    -Wno-deprecated-declarations
)

# Fix deprecated FreeRTOS types and macros
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    portTICK_RATE_MS=portTICK_PERIOD_MS
    xQueueHandle=QueueHandle_t
)
