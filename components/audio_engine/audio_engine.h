#pragma once

#include <stdint.h>
#include <stddef.h>   // <-- REQUIRED for size_t

typedef struct audio_sink {
    int (*write)(const uint8_t *data, size_t len);
    void (*start)(void);
    void (*stop)(void);
} audio_sink_t;

void audio_engine_init(audio_sink_t *out);
int audio_engine_write(const uint8_t *data, size_t len);

