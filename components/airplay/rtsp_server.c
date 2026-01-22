#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "rtsp_server";

static void handle_client(int client_sock)
{
    char *buffer = malloc(2048);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate RTSP buffer");
        close(client_sock);
        return;
    }
    while (1) {
        int len = recv(client_sock, buffer, 2047, 0);
        if (len <= 0) break;
        buffer[len] = '\0';

        ESP_LOGD(TAG, "Received RTSP request:\n%s", buffer);

        char *cseq_ptr = strstr(buffer, "CSeq: ");
        int cseq = 0;
        if (cseq_ptr) {
            cseq = atoi(cseq_ptr + 6);
        }

        char response[512];
        if (strncmp(buffer, "OPTIONS", 7) == 0) {
            snprintf(response, sizeof(response),
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %d\r\n"
                     "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, SET_PARAMETER, GET_PARAMETER\r\n"
                     "Server: AirTunes/105.1\r\n"
                     "\r\n", cseq);
        } else {
            snprintf(response, sizeof(response),
                     "RTSP/1.0 200 OK\r\n"
                     "CSeq: %d\r\n"
                     "Server: AirTunes/105.1\r\n"
                     "\r\n", cseq);
        }
        send(client_sock, response, strlen(response), 0);
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
            ESP_LOGI(TAG, "New RTSP client connection");
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
