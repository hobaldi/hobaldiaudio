#include "audio_engine.h"
#include "esp_a2dp_api.h"
#include "esp_bt.h"
#include "esp_log.h"

static const char *TAG = "hobaldi_bt";
static audio_engine_t *engine = NULL;

void bt_audio_data_cb(const uint8_t *data, uint32_t len)
{
    if (engine) {
        audio_engine_write(engine, data, len);
    }
}

void bt_init(audio_engine_t *audio)
{
    engine = audio;

    esp_a2d_register_callback(NULL);
    esp_a2d_sink_register_data_callback(bt_audio_data_cb);
    esp_a2d_sink_init();

    ESP_LOGI(TAG, "Bluetooth A2DP sink initialized");
}
