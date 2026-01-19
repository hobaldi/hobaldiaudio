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
    size_t available = audio_rb_get_filled(&engine_rb);
    size_t to_read = (available < len) ? available : len;
    
    // Ensure we only read whole 16-bit stereo samples (4 bytes)
    to_read &= ~3;
    
    if (to_read > 0) {
        return audio_rb_read(&engine_rb, data, to_read);
    }
    
    return 0;
}

