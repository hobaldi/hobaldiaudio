#include "driver/i2s.h"
#include "audio_engine.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2S_NUM I2S_NUM_0

static void i2s_task(void *arg) {
    uint8_t buffer[1024];
    size_t bytes_read, bytes_written;

    while (1) {
        bytes_read = audio_engine_read(buffer, sizeof(buffer));
        if (bytes_read > 0) {
            i2s_write(I2S_NUM, buffer, bytes_read, &bytes_written, portMAX_DELAY);
        }
    }
}

void i2s_out_init(void) {
    i2s_config_t config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = true,
    };

    i2s_pin_config_t pins = {
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = 22,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM, &config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pins);

    xTaskCreatePinnedToCore(i2s_task, "i2s_task", 4096, NULL, 5, NULL, 1);
}

