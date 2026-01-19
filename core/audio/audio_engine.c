#include "audio_engine.h"
#include "audio_ringbuf.h"

void audio_engine_init(audio_sink_t *out) {
    audio_rb_init(64 * 1024); // 64KB buffer (~170ms)
}

void audio_engine_write(const uint8_t *data, size_t len) {
    audio_rb_write(data, len);
}

size_t audio_engine_read(uint8_t *data, size_t len) {
    return audio_rb_read(data, len);
}

