#include "audio_ringbuf.h"
#include <stdlib.h>
#include <string.h>

static uint8_t *buffer;
static size_t buf_size;
static volatile size_t write_pos;
static volatile size_t read_pos;

void audio_rb_init(size_t size) {
    if (buffer) {
        free(buffer);
    }
    buffer = malloc(size);
    buf_size = size;
    write_pos = read_pos = 0;
}

size_t audio_rb_write(const uint8_t *data, size_t len) {
    if (!buffer || !data || buf_size == 0) return 0;
    
    size_t written = 0;
    while (written < len) {
        size_t next = (write_pos + 1) % buf_size;
        if (next == read_pos) break; // full
        buffer[write_pos] = data[written++];
        write_pos = next;
    }
    return written;
}

size_t audio_rb_read(uint8_t *data, size_t len) {
    if (!buffer || !data || buf_size == 0) return 0;

    size_t read = 0;
    while (read < len && read_pos != write_pos) {
        data[read++] = buffer[read_pos];
        read_pos = (read_pos + 1) % buf_size;
    }
    return read;
}

