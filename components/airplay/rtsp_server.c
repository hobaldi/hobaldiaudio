#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "raop_rtp.h"

static const char *TAG = "rtsp_server";

static void handle_client(int client_sock)
{
    char *buffer = malloc(4096);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate RTSP buffer");
        close(client_sock);
        return;
    }

    while (1) {
        int len = recv(client_sock, buffer, 4095, 0);
        if (len <= 0) break;
        buffer[len] = '\0';

        ESP_LOGI(TAG, "Received RTSP request:\n%s", buffer);

        // Parse CSeq
        int cseq = 0;
        char *cseq_ptr = strstr(buffer, "CSeq: ");
        if (cseq_ptr) {
            cseq = atoi(cseq_ptr + 6);
        }

        char *response = malloc(1024);
        int resp_len = 0;

        if (strncmp(buffer, "OPTIONS", 7) == 0) {
            resp_len = snprintf(response, 1024,
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %d\r\n"
                     "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, SET_PARAMETER, GET_PARAMETER\r\n"
                     "Server: AirTunes/105.1\r\n"
                     "\r\n", cseq);
        } else if (strncmp(buffer, "ANNOUNCE", 8) == 0) {
            // Handle ANNOUNCE (SDP can be parsed if needed, but we assume ALAC 44100/2/16)
            resp_len = snprintf(response, 1024,
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %d\r\n"
                     "Server: AirTunes/105.1\r\n"
                     "\r\n", cseq);
        } else if (strncmp(buffer, "SETUP", 5) == 0) {
            // Extract ports from Transport header
            // Transport: RTP/AVP/UDP;unicast;mode=record;control_port=6001;timing_port=6002
            int control_port = 0;
            int timing_port = 0;
            char *cp = strstr(buffer, "control_port=");
            if (cp) control_port = atoi(cp + 13);
            char *tp = strstr(buffer, "timing_port=");
            if (tp) timing_port = atoi(tp + 12);

            ESP_LOGI(TAG, "SETUP: control_port=%d, timing_port=%d", control_port, timing_port);

            // Rule 1: Be ready BEFORE RECORD. Start RTP/I2S now.
            raop_rtp_start(control_port, timing_port);

            // Respond with our ports.
            // In RAOP, data port is usually 5001, control 5002, timing 5003
            resp_len = snprintf(response, 1024,
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %d\r\n"
                     "Transport: RTP/AVP/UDP;unicast;mode=record;server_port=5001;control_port=5002;timing_port=5003\r\n"
                     "Session: 12345678\r\n"
                     "Server: AirTunes/105.1\r\n"
                     "\r\n", cseq);
        } else if (strncmp(buffer, "RECORD", 6) == 0) {
            resp_len = snprintf(response, 1024,
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %d\r\n"
                     "Audio-Latency: 2205\r\n"
                     "Server: AirTunes/105.1\r\n"
                     "\r\n", cseq);
        } else if (strncmp(buffer, "FLUSH", 5) == 0) {
            raop_rtp_flush();
            resp_len = snprintf(response, 1024,
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %d\r\n"
                     "Server: AirTunes/105.1\r\n"
                     "\r\n", cseq);
        } else if (strncmp(buffer, "TEARDOWN", 8) == 0) {
            raop_rtp_stop();
            resp_len = snprintf(response, 1024,
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %d\r\n"
                     "Connection: close\r\n"
                     "Server: AirTunes/105.1\r\n"
                     "\r\n", cseq);
        } else if (strncmp(buffer, "SET_PARAMETER", 13) == 0) {
            // Handle volume
            char *vol_ptr = strstr(buffer, "volume: ");
            if (vol_ptr) {
                float vol_db = atof(vol_ptr + 8);
                ESP_LOGI(TAG, "Volume set: %f dB", vol_db);
                raop_rtp_set_volume(vol_db);
            }
            resp_len = snprintf(response, 1024,
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %d\r\n"
                     "Server: AirTunes/105.1\r\n"
                     "\r\n", cseq);
        } else {
            // Default 200 OK for anything else (GET_PARAMETER, etc)
            resp_len = snprintf(response, 1024,
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %d\r\n"
                     "Server: AirTunes/105.1\r\n"
                     "\r\n", cseq);
        }

        if (resp_len > 0) {
            send(client_sock, response, resp_len, 0);
        }
        free(response);

        if (strncmp(buffer, "TEARDOWN", 8) == 0) break;
    }

    free(buffer);
    close(client_sock);
}

static void rtsp_client_task(void *pvParameters)
{
    int client_sock = (int)pvParameters;
    handle_client(client_sock);
    vTaskDelete(NULL);
}

static void rtsp_task(void *arg)
{
    int port = (int)arg;
    int server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (server_sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        vTaskDelete(NULL);
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: %d", errno);
        close(server_sock);
        vTaskDelete(NULL);
    }
    listen(server_sock, 5);

    ESP_LOGI(TAG, "RTSP server listening on port %d", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock >= 0) {
            ESP_LOGI(TAG, "New RTSP client connection from %s", inet_ntoa(client_addr.sin_addr));
            // Set receive timeout
            struct timeval tv;
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            if (xTaskCreate(rtsp_client_task, "rtsp_client", 4096, (void *)client_sock, 5, NULL) != pdPASS) {
                ESP_LOGE(TAG, "Failed to create RTSP client task");
                close(client_sock);
            }
        }
    }
}

void rtsp_server_start(int port)
{
    xTaskCreate(rtsp_task, "rtsp_server", 4096, (void *)port, 5, NULL);
}
