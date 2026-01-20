#include "audio_engine.h"
#include "esp_log.h"

static const char *TAG = "audio_engine";
static audio_sink_t *sink = NULL;

void audio_engine_init(audio_sink_t *out)
{
    sink = out;
    if (sink && sink->start) {
        sink->start();
    }
    ESP_LOGI(TAG, "Audio engine initialized");
}

int audio_engine_write(const uint8_t *data, size_t len)
{
    if (!sink || !sink->write) {
        return -1;
    }
    return sink->write(data, len);
}
