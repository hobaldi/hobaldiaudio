#include "audio_engine.h"
#include "buffer/audio_ringbuf.h"

static audio_ringbuf_t engine_rb;

void audio_engine_init(audio_sink_t *out) {
    (void)out;
    audio_rb_init(&engine_rb, 64 * 1024); // 64KB buffer (~170ms)
}

void audio_engine_write(const uint8_t *data, size_t len) {
    audio_rb_write(&engine_rb, data, len);
}

size_t audio_engine_read(uint8_t *data, size_t len) {
    return audio_rb_read(&engine_rb, data, len);
}
