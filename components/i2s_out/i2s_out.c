#include "i2s_out.h"
#include "esp_log.h"

static const char *TAG = "i2s_out";

static int i2s_write(const uint8_t *data, size_t len)
{
    // TODO: real i2s_write_bytes()
    return len;
}

static void i2s_start(void)
{
    ESP_LOGI(TAG, "I2S start");
}

static void i2s_stop(void)
{
    ESP_LOGI(TAG, "I2S stop");
}

static audio_sink_t sink = {
    .write = i2s_write,
    .start = i2s_start,
    .stop  = i2s_stop,
};

audio_sink_t *i2s_out_init(void)
{
    ESP_LOGI(TAG, "I2S sink initialized");
    return &sink;
}
