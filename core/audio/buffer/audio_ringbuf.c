#include "audio_ringbuf.h"
#include <stdlib.h>
#include <string.h>

int audio_rb_init(audio_ringbuf_t *rb, size_t size) {
    if (!rb) return -1;

    if (rb->buffer) {
        free(rb->buffer);
        rb->buffer = NULL;
    }

    rb->buffer = malloc(size);
    if (!rb->buffer) {
        rb->size = 0;
        atomic_init(&rb->write_pos, 0);
        atomic_init(&rb->read_pos, 0);
        return -1;
    }

    rb->size = size;
    atomic_init(&rb->write_pos, 0);
    atomic_init(&rb->read_pos, 0);
    return 0;
}

void audio_rb_deinit(audio_ringbuf_t *rb) {
    if (rb && rb->buffer) {
        free(rb->buffer);
        rb->buffer = NULL;
        rb->size = 0;
    }
}

size_t audio_rb_write(audio_ringbuf_t *rb, const uint8_t *data, size_t len) {
    if (!rb || !rb->buffer || !data || rb->size == 0) return 0;

    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_relaxed);
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);

    size_t free_space;
    if (write_pos >= read_pos) {
        free_space = rb->size - (write_pos - read_pos) - 1;
    } else {
        free_space = read_pos - write_pos - 1;
    }

    if (len > free_space) len = free_space;
    if (len == 0) return 0;

    size_t first_part = rb->size - write_pos;
    if (first_part > len) first_part = len;

    memcpy(&rb->buffer[write_pos], data, first_part);
    if (len > first_part) {
        memcpy(rb->buffer, &data[first_part], len - first_part);
    }

    atomic_store_explicit(&rb->write_pos, (write_pos + len) % rb->size, memory_order_release);
    return len;
}

size_t audio_rb_read(audio_ringbuf_t *rb, uint8_t *data, size_t len) {
    if (!rb || !rb->buffer || !data || rb->size == 0) return 0;

    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_relaxed);
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);

    size_t filled_space;
    if (write_pos >= read_pos) {
        filled_space = write_pos - read_pos;
    } else {
        filled_space = rb->size - (read_pos - write_pos);
    }

    if (len > filled_space) len = filled_space;
    if (len == 0) return 0;

    size_t first_part = rb->size - read_pos;
    if (first_part > len) first_part = len;

    memcpy(data, &rb->buffer[read_pos], first_part);
    if (len > first_part) {
        memcpy(&data[first_part], rb->buffer, len - first_part);
    }

    atomic_store_explicit(&rb->read_pos, (read_pos + len) % rb->size, memory_order_release);
    return len;
}
