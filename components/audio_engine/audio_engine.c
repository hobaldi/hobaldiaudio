#include "audio_engine.h"
#include "buffer/audio_ringbuf.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio_engine";
static audio_sink_t *current_sink = NULL;
static audio_ringbuf_t rb;
static TaskHandle_t engine_task_handle = NULL;

#define RING_BUFFER_SIZE (128 * 1024) // 128KB
#define READ_BUF_SIZE 4096

static void audio_engine_task(void *arg)
{
    uint8_t read_buf[READ_BUF_SIZE];
    ESP_LOGI(TAG, "Audio engine task started");

    while (1) {
        size_t filled = audio_rb_get_filled(&rb);
        if (filled < READ_BUF_SIZE) {
            // Not enough data, wait a bit or wait for a signal
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        size_t read = audio_rb_read(&rb, read_buf, READ_BUF_SIZE);
        if (read > 0) {
            if (current_sink && current_sink->write) {
                current_sink->write(read_buf, read);
            }
        }
    }
}

void audio_engine_init(audio_sink_t *out)
{
    current_sink = out;

    if (audio_rb_init(&rb, RING_BUFFER_SIZE) != 0) {
        ESP_LOGE(TAG, "Failed to initialize ring buffer");
        return;
    }

    if (current_sink && current_sink->start) {
        current_sink->start();
    }

    xTaskCreate(audio_engine_task, "audio_engine", 4096, NULL, 20, &engine_task_handle);

    ESP_LOGI(TAG, "Audio engine initialized with %d KB buffer", RING_BUFFER_SIZE / 1024);
}

int audio_engine_write(const uint8_t *data, size_t len)
{
    size_t written = audio_rb_write(&rb, data, len);
    if (written < len) {
        // Log sparingly to avoid flooding if buffer is full
        // ESP_LOGW(TAG, "Buffer overflow, dropped %d bytes", len - written);
    }
    return (int)written;
}
