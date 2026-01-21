#include "i2s_out.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "i2s_out";

static i2s_chan_handle_t tx_handle = NULL;

static int i2s_out_write(const uint8_t *data, size_t len)
{
    if (tx_handle == NULL) return -1;

    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(tx_handle, data, len, &bytes_written, portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_write failed: %d", err);
        return -1;
    }
    return (int)bytes_written;
}

static void i2s_out_start(void)
{
    if (tx_handle) {
        i2s_channel_enable(tx_handle);
        ESP_LOGI(TAG, "I2S channel enabled");
    }
}

static void i2s_out_stop(void)
{
    if (tx_handle) {
        i2s_channel_disable(tx_handle);
        ESP_LOGI(TAG, "I2S channel disabled");
    }
}

static audio_sink_t sink = {
    .write = i2s_out_write,
    .start = i2s_out_start,
    .stop  = i2s_out_stop,
};

audio_sink_t *i2s_out_init(void)
{
    ESP_LOGI(TAG, "Initializing I2S for PCM5102...");

    i2s_chan_config_t chan_cfg = I2S_STD_CHAN_CONFIG_DEFAULT(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_CONFIG(44100),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_CONFIG_16BIT(I2S_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_1,
            .ws = GPIO_NUM_2,
            .dout = GPIO_NUM_3,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));

    ESP_LOGI(TAG, "I2S initialized (BCLK:1, WS:2, DOUT:3)");
    return &sink;
}
