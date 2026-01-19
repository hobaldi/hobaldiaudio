#include "../core/audio/buffer/audio_ringbuf.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define DATA_SIZE 1000000
#define BUF_SIZE 1024

audio_ringbuf_t rb;
uint8_t src_data[DATA_SIZE];
uint8_t dst_data[DATA_SIZE];

void* producer(void* arg) {
    size_t sent = 0;
    while (sent < DATA_SIZE) {
        size_t to_send = (DATA_SIZE - sent > 100) ? 100 : DATA_SIZE - sent;
        size_t written = audio_rb_write(&rb, &src_data[sent], to_send);
        sent += written;
    }
    return NULL;
}

void* consumer(void* arg) {
    size_t received = 0;
    while (received < DATA_SIZE) {
        uint8_t buf[128];
        size_t read = audio_rb_read(&rb, buf, 128);
        for (size_t i = 0; i < read; i++) {
            dst_data[received + i] = buf[i];
        }
        received += read;
    }
    return NULL;
}

int main() {
    audio_rb_init(&rb, BUF_SIZE);
    for (int i = 0; i < DATA_SIZE; i++) src_data[i] = i % 256;

    pthread_t t1, t2;
    pthread_create(&t1, NULL, producer, NULL);
    pthread_create(&t2, NULL, consumer, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    for (int i = 0; i < DATA_SIZE; i++) {
        if (src_data[i] != dst_data[i]) {
            printf("Mismatch at %d: %d != %d\n", i, src_data[i], dst_data[i]);
            return 1;
        }
    }

    printf("Concurrency test passed\n");
    audio_rb_deinit(&rb);
    return 0;
}
