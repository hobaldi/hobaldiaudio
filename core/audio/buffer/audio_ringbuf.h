#pragma once
#include <stdint.h>
#include <stddef.h>

void audio_rb_init(size_t size);
size_t audio_rb_write(const uint8_t *data, size_t len);
size_t audio_rb_read(uint8_t *data, size_t len);

