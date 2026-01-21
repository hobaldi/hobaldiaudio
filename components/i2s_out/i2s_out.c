#include "i2s_out.h"
#include "driver/i2s.h"
#include "esp_log.h"

static const char *TAG = "i2s_out";
#define I2S_NUM I2S_NUM_0

static int i2s_out_write(const uint8_t *data, size_t len)
{
    size_t bytes_written = 0;
    esp_err_t err = i2s_write(I2S_NUM, data, len, &bytes_written, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_write failed: %d", err);
        return -1;
    }
    return (int)bytes_written;
}

static void i2s_out_start(void)
{
    i2s_start(I2S_NUM);
    ESP_LOGI(TAG, "I2S started");
}

static void i2s_out_stop(void)
{
    i2s_stop(I2S_NUM);
    ESP_LOGI(TAG, "I2S stopped");
}

static audio_sink_t sink = {
    .write = i2s_out_write,
    .start = i2s_out_start,
    .stop  = i2s_out_stop,
};

audio_sink_t *i2s_out_init(void)
{
    ESP_LOGI(TAG, "Initializing I2S (legacy) for PCM5102...");

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = 1,
        .ws_io_num = 2,
        .data_out_num = 3,
        .data_in_num = -1,
    };

    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM, &pin_config));

    ESP_LOGI(TAG, "I2S initialized (BCLK:1, WS:2, DOUT:3)");
    return &sink;
}
