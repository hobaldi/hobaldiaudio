#pragma once
#include <stdint.h>
#include <stddef.h>

struct audio_sink;
typedef struct audio_sink audio_sink_t;

void audio_engine_init(audio_sink_t *out);
void audio_engine_write(const uint8_t *data, size_t len);
size_t audio_engine_read(uint8_t *data, size_t len);
