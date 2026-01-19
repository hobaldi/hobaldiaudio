#include "audio_engine.h"

void app_main(void)
{
    audio_format_t fmt = {
        .sample_rate = 44100,
        .channels = 2,
        .bit_depth = 16
    };

    audio_engine_t *engine = audio_engine_init(&fmt);

    // Bluetooth / Spotify will feed PCM here

    while (1) {
        // main loop
    }
}
