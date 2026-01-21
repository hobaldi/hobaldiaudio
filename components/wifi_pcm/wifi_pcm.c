#include "wifi_pcm.h"
#include "audio_engine.h"

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#define PCM_PORT 1234
#define BUF_SIZE 1460 // Max UDP payload for standard Ethernet MTU

static const char *TAG = "wifi_pcm";

static void wifi_pcm_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket failed: %d", errno);
        vTaskDelete(NULL);
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PCM_PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind failed: %d", errno);
        close(sock);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Listening UDP PCM on port %d", PCM_PORT);

    uint8_t *buf = malloc(BUF_SIZE);
    if (!buf) {
        ESP_LOGE(TAG, "failed to allocate recv buffer");
        close(sock);
        vTaskDelete(NULL);
    }

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, buf, BUF_SIZE, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: %d", errno);
            break;
        }

        if (len > 0) {
            audio_engine_write(buf, len);
        }
    }

    free(buf);
    close(sock);
    vTaskDelete(NULL);
}

void wifi_pcm_start(void)
{
    // High priority for networking task
    xTaskCreate(wifi_pcm_task, "wifi_pcm", 4096, NULL, 15, NULL);
}
