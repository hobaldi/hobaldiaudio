#include "audio_engine.h"

void bt_init(audio_engine_t *audio);

void app_main(void)
{
    audio_format_t fmt = {
        .sample_rate = 44100,
        .channels = 2,
        .bit_depth = 16
    };

    audio_engine_t *engine = audio_engine_init(&fmt);

    bt_init(engine);

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

