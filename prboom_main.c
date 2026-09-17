// ESP32-S3 DOOM for Disobey Badge 2025/2026
// Based on ESP32-DOOM by AmirhoseinMasoumi / doom-espidf by jkirsons

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"

static const char *TAG = "DOOM";

extern void jsInit(void);
extern int doom_main(int argc, char const * const *argv);
extern void spi_lcd_init(void);

void doomEngineTask(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting DOOM engine...");
    ESP_LOGI(TAG, "Free DRAM: %lu bytes", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "Free PSRAM: %lu bytes", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    char const *argv[] = {"doom", "-cout", "ICWEFDA", NULL};
    doom_main(3, argv);

    ESP_LOGI(TAG, "DOOM engine exited");
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Disobey Badge 2025/2026 - DOOM Initializing...");

    if (esp_psram_get_size() > 0) {
        ESP_LOGI(TAG, "PSRAM size: %d MB", esp_psram_get_size() / (1024 * 1024));
    } else {
        ESP_LOGW(TAG, "No PSRAM detected!");
    }

    // Initialize display
    spi_lcd_init();

    // Initialize badge button input
    jsInit();

    // Start DOOM engine on core 0 with large stack
    xTaskCreatePinnedToCore(
        &doomEngineTask,
        "doomEngine",
        32768,
        NULL,
        5,
        NULL,
        0
    );
}
