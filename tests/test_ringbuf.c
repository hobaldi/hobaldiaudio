#include "../core/audio/buffer/audio_ringbuf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_basic() {
    audio_ringbuf_t rb = {0};
    audio_rb_init(&rb, 10);
    uint8_t data[] = {1, 2, 3, 4, 5};
    size_t written = audio_rb_write(&rb, data, 5);
    assert(written == 5);
    assert(audio_rb_get_filled(&rb) == 5);

    uint8_t out[5];
    size_t read = audio_rb_read(&rb, out, 5);
    assert(read == 5);
    assert(memcmp(data, out, 5) == 0);
    assert(audio_rb_get_filled(&rb) == 0);
    audio_rb_deinit(&rb);
    printf("test_basic passed\n");
}

void test_wrap_around() {
    audio_ringbuf_t rb = {0};
    audio_rb_init(&rb, 5); // can hold 4 bytes
    uint8_t data[] = {1, 2, 3};
    audio_rb_write(&rb, data, 3);

    uint8_t out[2];
    audio_rb_read(&rb, out, 2);
    assert(out[0] == 1 && out[1] == 2);

    uint8_t data2[] = {4, 5};
    size_t written = audio_rb_write(&rb, data2, 2);
    assert(written == 2);

    uint8_t out2[3];
    size_t read = audio_rb_read(&rb, out2, 3);
    assert(read == 3);
    assert(out2[0] == 3 && out2[1] == 4 && out2[2] == 5);
    audio_rb_deinit(&rb);
    printf("test_wrap_around passed\n");
}

int main() {
    test_basic();
    test_wrap_around();
    return 0;
}
