#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "audio_engine.h"
#include "i2s_out.h"
#include "wifi_pcm.h"
#include "airplay.h"

// Forward declaration from wifi.c
void wifi_init_sta(void);

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Hobaldi for ESP32-S3...");

    // Initialize NVS (needed for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. Initialize Audio early
    // PCM5102 Pins: BCLK=1, WS=2, DOUT=3
    audio_sink_t *i2s_sink = i2s_out_init();
    audio_engine_init(i2s_sink);

    // 2. Initialize WiFi (Now non-blocking)
    wifi_init_sta();

    // 3. Start streaming services
    // These will start listening but will only be accessible once WiFi is connected
    wifi_pcm_start();
    airplay_start("HobaldiStreamer");

    ESP_LOGI(TAG, "Initialization complete. Ready for PCM or AirPlay stream.");
}
