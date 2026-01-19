#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t sample_rate;
    uint8_t  channels;
    uint8_t  bit_depth;
} audio_format_t;

typedef struct audio_engine audio_engine_t;

// Initialize audio engine
audio_engine_t* audio_engine_init(audio_format_t *fmt);

// Push PCM frames into engine
size_t audio_engine_write(audio_engine_t *engine, const void *data, size_t bytes);

// Set volume (0.0 - 1.0)
void audio_engine_set_volume(audio_engine_t *engine, float volume);

// Shutdown
void audio_engine_deinit(audio_engine_t *engine);
