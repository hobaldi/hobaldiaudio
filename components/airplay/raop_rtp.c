#include "raop_rtp.h"
#include "alac.h"
#include "audio_engine.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

static const char *TAG = "raop_rtp";

#define DATA_PORT 5001
#define CONTROL_PORT 5002
#define TIMING_PORT 5003

#define MAX_PACKET_SIZE 1500
#define JITTER_BUFFER_SIZE 8

typedef struct {
    uint16_t seq;
    uint8_t payload[MAX_PACKET_SIZE];
    int payload_len;
    bool occupied;
} jitter_item_t;

static jitter_item_t jitter_buffer[JITTER_BUFFER_SIZE];
static uint16_t next_seq = 0;
static bool first_packet = true;
static float volume_gain = 1.0f;
static alac_file *decoder = NULL;
static bool running = false;

static int data_sock = -1;
static int control_sock = -1;
static int timing_sock = -1;

// RTP Header
typedef struct {
    uint8_t flags;
    uint8_t payload_type;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t ssrc;
} rtp_header_t;

void raop_rtp_set_volume(float volume_db) {
    if (volume_db <= -144.0f) {
        volume_gain = 0.0f;
    } else {
        volume_gain = powf(10.0f, volume_db / 20.0f);
    }
    ESP_LOGI(TAG, "Volume gain set to %f", volume_gain);
}

static void apply_volume(int16_t *samples, int num_samples) {
    if (volume_gain >= 0.999f) return;
    if (volume_gain <= 0.001f) {
        memset(samples, 0, num_samples * 4);
        return;
    }
    for (int i = 0; i < num_samples * 2; i++) {
        samples[i] = (int16_t)(samples[i] * volume_gain);
    }
}

static void timing_task(void *pv) {
    uint8_t buf[128];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    ESP_LOGI(TAG, "Timing task started on port %d", TIMING_PORT);
    while (running) {
        int len = recvfrom(timing_sock, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (len < 0) continue;

        if (len >= 32) {
            uint64_t now_us = esp_timer_get_time();
            uint32_t now_s = now_us / 1000000;
            uint32_t now_frac = (now_us % 1000000) * 4294;

            uint8_t reply[32];
            memset(reply, 0, 32);
            reply[0] = 0x80;
            reply[1] = 0xd3;
            reply[3] = 0x07;

            memcpy(reply + 8, buf + 24, 8);

            uint32_t s = htonl(now_s);
            uint32_t f = htonl(now_frac);
            memcpy(reply + 16, &s, 4);
            memcpy(reply + 20, &f, 4);
            memcpy(reply + 24, &s, 4);
            memcpy(reply + 28, &f, 4);

            sendto(timing_sock, reply, 32, 0, (struct sockaddr *)&client_addr, addr_len);
        }
    }
    vTaskDelete(NULL);
}

static void control_task(void *pv) {
    uint8_t buf[MAX_PACKET_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    ESP_LOGI(TAG, "Control task started on port %d", CONTROL_PORT);
    while (running) {
        int len = recvfrom(control_sock, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (len < 0) continue;
    }
    vTaskDelete(NULL);
}

static void decode_and_play(uint8_t *payload, int payload_len) {
    if (!decoder) return;
    static int16_t pcm_out[4096];
    int decoded_size = sizeof(pcm_out);
    alac_decode_frame(decoder, payload, pcm_out, &decoded_size);

    if (decoded_size > 0) {
        apply_volume(pcm_out, decoded_size / 4);
        audio_engine_write((uint8_t *)pcm_out, decoded_size);
    }
}

static void data_task(void *pv) {
    uint8_t *packet_buf = malloc(MAX_PACKET_SIZE);
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    ESP_LOGI(TAG, "Data task started on port %d", DATA_PORT);
    while (running) {
        int len = recvfrom(data_sock, packet_buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (len < 0) continue;

        if (len < sizeof(rtp_header_t)) continue;
        rtp_header_t *hdr = (rtp_header_t *)packet_buf;
        uint16_t seq = ntohs(hdr->seq);
        uint8_t *payload = packet_buf + sizeof(rtp_header_t);
        int payload_len = len - sizeof(rtp_header_t);

        if (first_packet) {
            next_seq = seq;
            first_packet = false;
        }

        int16_t diff = (int16_t)(seq - next_seq);

        if (diff < 0) {
            // Late packet, drop it
            continue;
        } else if (diff == 0) {
            // Correct packet
            decode_and_play(payload, payload_len);
            next_seq++;

            // Check if we have next packets in jitter buffer
            while (true) {
                int next_idx = next_seq % JITTER_BUFFER_SIZE;
                if (jitter_buffer[next_idx].occupied && jitter_buffer[next_idx].seq == next_seq) {
                    decode_and_play(jitter_buffer[next_idx].payload, jitter_buffer[next_idx].payload_len);
                    jitter_buffer[next_idx].occupied = false;
                    next_seq++;
                } else {
                    break;
                }
            }
        } else if (diff < JITTER_BUFFER_SIZE) {
            // Future packet, store in jitter buffer
            int idx = seq % JITTER_BUFFER_SIZE;
            jitter_buffer[idx].seq = seq;
            jitter_buffer[idx].payload_len = payload_len;
            memcpy(jitter_buffer[idx].payload, payload, payload_len);
            jitter_buffer[idx].occupied = true;
        } else {
            // Too far in the future, reset and play
            ESP_LOGW(TAG, "Gap too large, resetting jitter buffer. Seq: %u, Next: %u", seq, next_seq);
            for(int i=0; i<JITTER_BUFFER_SIZE; i++) jitter_buffer[i].occupied = false;
            decode_and_play(payload, payload_len);
            next_seq = seq + 1;
        }
    }
    free(packet_buf);
    vTaskDelete(NULL);
}

static int create_udp_socket(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY,
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return sock;
}

void raop_rtp_start(int control_port, int timing_port) {
    if (running) return;
    running = true;
    first_packet = true;
    for(int i=0; i<JITTER_BUFFER_SIZE; i++) jitter_buffer[i].occupied = false;

    if (!decoder) {
        decoder = alac_create(16, 2);
        decoder->setinfo_max_samples_per_frame = 352;
        decoder->setinfo_sample_size = 16;
        decoder->setinfo_rice_historymult = 40;
        decoder->setinfo_rice_initialhistory = 10;
        decoder->setinfo_rice_kmodifier = 14;
        decoder->setinfo_8a_rate = 44100;
        alac_allocate_buffers(decoder);
    }

    data_sock = create_udp_socket(DATA_PORT);
    control_sock = create_udp_socket(CONTROL_PORT);
    timing_sock = create_udp_socket(TIMING_PORT);

    xTaskCreate(data_task, "raop_data", 8192, NULL, 15, NULL);
    xTaskCreate(control_task, "raop_ctrl", 4096, NULL, 10, NULL);
    xTaskCreate(timing_task, "raop_time", 4096, NULL, 10, NULL);

    uint8_t silence[1024] = {0};
    for(int i=0; i<10; i++) audio_engine_write(silence, sizeof(silence));

    ESP_LOGI(TAG, "RAOP RTP started");
}

void raop_rtp_stop(void) {
    running = false;
    vTaskDelay(pdMS_TO_TICKS(1100));
    if (data_sock != -1) close(data_sock);
    if (control_sock != -1) close(control_sock);
    if (timing_sock != -1) close(timing_sock);
    data_sock = control_sock = timing_sock = -1;
    ESP_LOGI(TAG, "RAOP RTP stopped");
}

void raop_rtp_flush(void) {
    first_packet = true;
    for(int i=0; i<JITTER_BUFFER_SIZE; i++) jitter_buffer[i].occupied = false;
}
