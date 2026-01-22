#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "mdns.h"

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

static esp_err_t setup_get_handler(httpd_req_t *req)
{
    char *buf = malloc(8192);
    if (!buf) return ESP_FAIL;
    int p = 0;

    p += sprintf(buf + p, "<html><head><title>HobaldiStreamer</title><meta name='viewport' content='width=device-width, initial-scale=1'>"
                 "<style>body{font-family:sans-serif;padding:20px;background-color:#f0f0f0;} .card{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);} input, select{width:100%%;padding:10px;margin:5px 0;box-sizing:border-box; border:1px solid #ccc; border-radius:4px;} input[type=submit], .btn{display:block;width:100%%;padding:12px;margin:10px 0;box-sizing:border-box; border:none; border-radius:4px; text-align:center; text-decoration:none; font-size:16px; cursor:pointer;} .btn-primary{background-color:#4CAF50;color:white;} .btn-danger{background-color:#f44336;color:white;} .status{padding:10px;margin-bottom:20px;border-radius:5px;} .connected{background-color:#dff0d8;color:#3c763d;} .disconnected{background-color:#f2dede;color:#a94442;} .streaming-info{background-color:#e3f2fd;padding:15px;border-radius:8px;margin-top:10px;border-left:5px solid #2196F3;}</style></head><body><div class='card'><h1>HobaldiStreamer</h1>");

    wifi_ap_record_t ap_info;
    bool connected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);

    if (connected) {
        // Connected Home Page
        p += sprintf(buf + p, "<div class='status connected'>Status: <b>Connected to %s</b></div>", ap_info.ssid);
        p += sprintf(buf + p, "<div class='streaming-info'><h3>Streaming Status</h3>"
                     "<p>Device is <b>Discoverable</b> on your network.</p>"
                     "<ul>"
                     "<li><b>iPhone/Mac (AirPlay):</b> Select 'HobaldiStreamer'</li>"
                     "<li><b>Android/Tidal:</b> Use <b>AirMusic</b> or <b>BubbleUPnP</b></li>"
                     "<li><b>Advanced:</b> Raw UDP PCM on port 1234</li>"
                     "</ul>"
                     "</div>");

        p += sprintf(buf + p, "<div style='margin-top:20px; font-size:0.9em; color:#555;'>"
                     "<h4>How to stream from Android/Tidal:</h4>"
                     "<p>Tidal on Android doesn't support AirPlay or DLNA directly. We recommend:</p>"
                     "<ol>"
                     "<li>Install <b>AirMusic</b> to stream <i>any</i> app (like Tidal) to this device via AirPlay.</li>"
                     "<li>Or use <b>BubbleUPnP</b> to stream Tidal via DLNA/UPnP.</li>"
                     "</ol></div>");

        p += sprintf(buf + p, "<hr><p>To change WiFi network or reset settings:</p>"
                     "<form method='POST' action='/disconnect'><input type='submit' class='btn btn-danger' value='Disconnect/Change WiFi'></form>");
    } else {
        // Setup Page
        p += sprintf(buf + p, "<div class='status disconnected'>Status: Not Connected</div>");
        p += sprintf(buf + p, "<p>Select a WiFi network to start streaming.</p>"
                     "<form method='POST' action='/setup'>"
                     "Available SSIDs:<br><select name='ssid' onchange='if(this.value==\"_manual\") {document.getElementById(\"manual_ssid\").style.display=\"block\";} else {document.getElementById(\"manual_ssid\").style.display=\"none\";}'>");

        // Scan for SSIDs
        uint16_t number = 20;
        wifi_ap_record_t ap_records[20];
        wifi_scan_config_t scan_config = { .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = false };
        esp_wifi_scan_start(&scan_config, true);
        esp_wifi_scan_get_ap_records(&number, ap_records);

        for (int i = 0; i < number; i++) {
            p += sprintf(buf + p, "<option value='%s'>%s (RSSI: %d)</option>", (char*)ap_records[i].ssid, (char*)ap_records[i].ssid, ap_records[i].rssi);
        }
        p += sprintf(buf + p, "<option value='_manual'>[Enter Manually]</option></select><br>");
        p += sprintf(buf + p, "<div id='manual_ssid' style='display:none;'>Manual SSID:<br><input type='text' name='manual_ssid' placeholder='WiFi Name'></div>");

        p += sprintf(buf + p, "Password:<br><input type='password' name='password' placeholder='Password'><br><br>"
                     "<input type='submit' class='btn btn-primary' value='Save and Connect'>"
                     "</form>");
    }

    p += sprintf(buf + p, "</div><p style='font-size:0.8em;color:#666;'>Portal: http://hobaldistreamer.local or http://192.168.4.1</p></body></html>");

    httpd_resp_send(req, buf, strlen(buf));
    free(buf);
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
    char manual_ssid[64] = {0};
    char raw_pass[64] = {0};
    char ssid[64] = {0};
    char pass[64] = {0};

    char *s = strstr(buf, "ssid=");
    if (s) {
        s += 5;
        char *end = strchr(s, '&');
        if (end) *end = '\0';
        strncpy(raw_ssid, s, sizeof(raw_ssid)-1);
        if (end) *end = '&';
    }

    char *m = strstr(buf, "manual_ssid=");
    if (m) {
        m += 12;
        char *end = strchr(m, '&');
        if (end) *end = '\0';
        strncpy(manual_ssid, m, sizeof(manual_ssid)-1);
        if (end) *end = '&';
    }

    char *p = strstr(buf, "password=");
    if (p) {
        p += 9;
        strncpy(raw_pass, p, sizeof(raw_pass)-1);
    }

    if (strcmp(raw_ssid, "_manual") == 0) {
        url_decode(ssid, manual_ssid);
    } else {
        url_decode(ssid, raw_ssid);
    }
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

    const char *resp = "<html><head><meta http-equiv='refresh' content='10;url=http://hobaldistreamer.local/'></head>"
                       "<body><h1>Credentials saved!</h1><p>Rebooting to connect to WiFi...</p>"
                       "<p>In 10 seconds, this page will try to redirect to <a href='http://hobaldistreamer.local/'>http://hobaldistreamer.local/</a></p>"
                       "<p>If it doesn't work, make sure your phone is on the SAME WiFi network as the device.</p></body></html>";
    httpd_resp_send(req, resp, strlen(resp));
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

static esp_err_t disconnect_handler(httpd_req_t *req)
{
    nvs_handle_t nvs;
    if (nvs_open("storage", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_erase_key(nvs, "wifi_ssid");
        nvs_erase_key(nvs, "wifi_pass");
        nvs_commit(nvs);
        nvs_close(nvs);
    }
    // Also clear WiFi driver's internal storage
    esp_wifi_restore();

    const char *resp = "<html><body><h1>WiFi credentials erased!</h1>"
                       "<p>The device is rebooting to <b>Setup Mode</b>.</p>"
                       "<p>Please connect your phone to the <b>'Hobaldi-Setup'</b> WiFi network to configure it again.</p>"
                       "<p>Then go to <a href='http://192.168.4.1/'>http://192.168.4.1/</a></p></body></html>";
    httpd_resp_send(req, resp, strlen(resp));
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

static esp_err_t description_xml_handler(httpd_req_t *req)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char buf[512];
    snprintf(buf, sizeof(buf),
             "<?xml version='1.0'?>"
             "<root xmlns='urn:schemas-upnp-org:device-1-0'>"
             "<specVersion><major>1</major><minor>0</minor></specVersion>"
             "<device>"
             "<deviceType>urn:schemas-upnp-org:device:MediaRenderer:1</deviceType>"
             "<friendlyName>HobaldiStreamer</friendlyName>"
             "<manufacturer>Hobaldi</manufacturer>"
             "<modelName>Hobaldi S3</modelName>"
             "<UDN>uuid:52316854-0231-9852-58%02x-%02x%02x%02x%02x%02x%02x</UDN>"
             "</device></root>",
             mac[0], mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    httpd_resp_set_type(req, "text/xml");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

/* Captive Portal Redirect Handler */
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Redirect all 404 errors to the root page (setup page)
    ESP_LOGI(TAG, "Redirecting 404 to root for URI: %s", req->uri);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static void start_webserver(void)
{
    if (server != NULL) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 12288; // Increased for HTML generation
    config.lru_purge_enable = true;
    config.ctrl_port = 32768; // Avoid conflicts

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t setup_get = { .uri = "/", .method = HTTP_GET, .handler = setup_get_handler };
        httpd_register_uri_handler(server, &setup_get);

        httpd_uri_t setup_post = { .uri = "/setup", .method = HTTP_POST, .handler = setup_post_handler };
        httpd_register_uri_handler(server, &setup_post);

        httpd_uri_t disconnect_post = { .uri = "/disconnect", .method = HTTP_POST, .handler = disconnect_handler };
        httpd_register_uri_handler(server, &disconnect_post);

        httpd_uri_t disconnect_get = { .uri = "/disconnect", .method = HTTP_GET, .handler = disconnect_handler };
        httpd_register_uri_handler(server, &disconnect_get);

        httpd_uri_t description_xml = { .uri = "/description.xml", .method = HTTP_GET, .handler = description_xml_handler };
        httpd_register_uri_handler(server, &description_xml);

        // Register 404 error handler for captive portal redirection
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
}

/* DNS Server Task for Captive Portal */
static void dns_server_task(void *pvParameters)
{
    uint8_t data[512];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create DNS socket");
        vTaskDelete(NULL);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(53);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind DNS socket");
        close(sock);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "DNS server started on port 53");

    while (1) {
        int len = recvfrom(sock, data, sizeof(data), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        // Safety check to prevent buffer overflow when appending answer
        if (len > 12 && len < (sizeof(data) - 16)) {
            // Very basic DNS responder: Always return 192.168.4.1 for any A query
            data[2] |= 0x80; // QR = 1 (Response)
            data[3] |= 0x80; // RA = 1 (Recursion Available)
            data[7] = 1;     // ANCOUNT = 1

            int pos = len;
            data[pos++] = 0xc0; // Name pointer
            data[pos++] = 0x0c;
            data[pos++] = 0x00; data[pos++] = 0x01; // Type A
            data[pos++] = 0x00; data[pos++] = 0x01; // Class IN
            data[pos++] = 0x00; data[pos++] = 0x00; data[pos++] = 0x00; data[pos++] = 0x3c; // TTL 60s
            data[pos++] = 0x00; data[pos++] = 0x04; // RDLENGTH 4
            data[pos++] = 192; data[pos++] = 168; data[pos++] = 4; data[pos++] = 1; // 192.168.4.1

            sendto(sock, data, pos, 0, (struct sockaddr *)&client_addr, client_addr_len);
        }
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying connection to AP (%d/10)...", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        // Security enhancement: ensure SoftAP is disabled once connected to home WiFi
        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) == ESP_OK) {
            if (mode == WIFI_MODE_APSTA) {
                ESP_LOGI(TAG, "Switching to STA mode to disable setup AP for security.");
                esp_wifi_set_mode(WIFI_MODE_STA);
            }
        }

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void start_softap_setup(void)
{
    ESP_LOGI(TAG, "Starting SoftAP 'Hobaldi-Setup'...");
    esp_wifi_stop();

    if (ap_netif == NULL) {
        ap_netif = esp_netif_create_default_wifi_ap();
    }

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

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    start_webserver();
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Connect to 'Hobaldi-Setup' to configure WiFi.");
    ESP_LOGI(TAG, "Setup page also available at http://hobaldistreamer.local when connected.");
}

static void wifi_manager_task(void *pvParameters)
{
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

        // Wait up to 10 seconds for connection
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected successfully.");
            start_webserver(); // Start webserver even in STA mode for later adjustments
            vTaskDelete(NULL);
            return;
        } else {
            ESP_LOGW(TAG, "Failed to connect to stored WiFi. Falling back to setup mode.");
        }
    }

    // Fallback to setup mode
    start_softap_setup();

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        ESP_LOGI(TAG, "Waiting for WiFi configuration in setup mode...");
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

    // Initialize mDNS with a consistent hostname
    esp_err_t err = mdns_init();
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        mdns_hostname_set("HobaldiStreamer");
    }

    // Start WiFi management in the background
    xTaskCreate(wifi_manager_task, "wifi_mgr", 6144, NULL, 5, NULL);
}
