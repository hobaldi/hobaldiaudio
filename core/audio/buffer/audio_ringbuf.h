#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>

typedef struct {
    uint8_t *buffer;
    size_t size;
    atomic_size_t write_pos;
    atomic_size_t read_pos;
} audio_ringbuf_t;

int audio_rb_init(audio_ringbuf_t *rb, size_t size);
void audio_rb_deinit(audio_ringbuf_t *rb);
size_t audio_rb_write(audio_ringbuf_t *rb, const uint8_t *data, size_t len);
size_t audio_rb_read(audio_ringbuf_t *rb, uint8_t *data, size_t len);
size_t audio_rb_get_filled(audio_ringbuf_t *rb);
