#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_server.h"

static const char *TAG = "wifi_mgr";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static httpd_handle_t server = NULL;
static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;

/* Simple URL decoding */
static void url_decode(char *dst, const char *src)
{
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/* HTML for the setup page */
static const char* setup_html =
"<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
"<style>body{font-family:sans-serif;padding:20px;}input{width:100%;padding:10px;margin:5px 0;}</style></head>"
"<body><h1>Hobaldi WiFi Setup</h1>"
"<form method='POST' action='/setup'>"
"SSID:<br><input type='text' name='ssid'><br>"
"Password:<br><input type='password' name='password'><br><br>"
"<input type='submit' value='Save and Connect'>"
"</form></body></html>";

static esp_err_t setup_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, setup_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t setup_post_handler(httpd_req_t *req)
{
    char buf[512];
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buf)) remaining = sizeof(buf) - 1;

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    char raw_ssid[64] = {0};
    char raw_pass[64] = {0};
    char ssid[33] = {0};
    char pass[64] = {0};

    char *s = strstr(buf, "ssid=");
    if (s) {
        s += 5;
        char *end = strchr(s, '&');
        if (end) *end = '\0';
        strncpy(raw_ssid, s, sizeof(raw_ssid)-1);
        if (end) *end = '&';
    }

    char *p = strstr(buf, "password=");
    if (p) {
        p += 9;
        strncpy(raw_pass, p, sizeof(raw_pass)-1);
    }

    url_decode(ssid, raw_ssid);
    url_decode(pass, raw_pass);

    ESP_LOGI(TAG, "Received credentials for SSID: %s", ssid);

    // Save to NVS
    nvs_handle_t nvs;
    if (nvs_open("storage", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "wifi_ssid", ssid);
        nvs_set_str(nvs, "wifi_pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    httpd_resp_sendstr(req, "<h1>Credentials saved! Rebooting...</h1>");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t setup_get = { .uri = "/", .method = HTTP_GET, .handler = setup_get_handler };
        httpd_register_uri_handler(server, &setup_get);
        httpd_uri_t setup_post = { .uri = "/setup", .method = HTTP_POST, .handler = setup_post_handler };
        httpd_register_uri_handler(server, &setup_post);
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying connection to AP...");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    nvs_handle_t nvs;
    char ssid[33] = {0};
    char pass[64] = {0};
    size_t size;

    bool has_creds = false;
    if (nvs_open("storage", NVS_READONLY, &nvs) == ESP_OK) {
        size = sizeof(ssid);
        if (nvs_get_str(nvs, "wifi_ssid", ssid, &size) == ESP_OK) {
            size = sizeof(pass);
            nvs_get_str(nvs, "wifi_pass", pass, &size);
            has_creds = true;
        }
        nvs_close(nvs);
    }

    if (has_creds) {
        wifi_config_t wifi_config = {0};
        strncpy((char*)wifi_config.sta.ssid, ssid, 32);
        strncpy((char*)wifi_config.sta.password, pass, 63);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "Attempting to connect to SSID: %s", ssid);

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected successfully.");
            return;
        } else {
            ESP_LOGW(TAG, "Failed to connect to stored WiFi.");
        }
    }

    // Fallback to SoftAP
    ESP_LOGI(TAG, "Starting SoftAP 'Hobaldi-Setup'...");
    esp_wifi_stop();
    ap_netif = esp_netif_create_default_wifi_ap();

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "Hobaldi-Setup",
            .ssid_len = strlen("Hobaldi-Setup"),
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    start_webserver();
    ESP_LOGI(TAG, "Connect to 'Hobaldi-Setup' to configure WiFi.");

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Waiting for WiFi configuration...");
    }
}
