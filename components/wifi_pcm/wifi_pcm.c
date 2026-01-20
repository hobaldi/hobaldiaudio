#include "wifi_pcm.h"
#include "audio_engine.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#define PCM_PORT 1234
#define BUF_SIZE 1024

static const char *TAG = "wifi_pcm";

static void wifi_pcm_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket failed");
        vTaskDelete(NULL);
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PCM_PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    ESP_LOGI(TAG, "Listening UDP PCM on port %d", PCM_PORT);

    uint8_t buf[BUF_SIZE];

    while (1) {
        int len = recv(sock, buf, sizeof(buf), 0);
        if (len > 0) {
            audio_engine_write(buf, len);
        }
    }
}

void wifi_pcm_start(void)
{
    xTaskCreate(wifi_pcm_task, "wifi_pcm", 4096, NULL, 5, NULL);
}
