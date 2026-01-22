#include "airplay.h"
#include "mdns.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <string.h>

static const char *TAG = "airplay";
static char saved_device_name[64] = "HobaldiStreamer";

// Forward declaration for RTSP server
void rtsp_server_start(int port);

static void register_mdns_services(const char *device_name)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    // MAC without colons, LOWERCASE (Crucial for AirPort Express / RAOP)
    char mac_str[13];
    snprintf(mac_str, sizeof(mac_str), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char raop_service_name[64];
    snprintf(raop_service_name, sizeof(raop_service_name), "%s@%s", mac_str, device_name);

    // RAOP (Remote Audio Output Protocol) advertisement - mimicking AirPort Express
    mdns_txt_item_t raop_txt[] = {
        {"txtvers", "1"},
        {"ch", "2"},
        {"cn", "0,1"},
        {"da", "true"},
        {"et", "0,1"},
        {"md", "AirPortExpress1,1"},
        {"pw", "false"},
        {"sr", "44100"},
        {"ss", "16"},
        {"tp", "UDP"},
        {"vn", "65537"},
        {"vs", "105.1"},
    };

    // Remove if already exists
    mdns_service_remove("_raop", "_tcp");
    mdns_service_add(raop_service_name, "_raop", "_tcp", 5000, raop_txt, sizeof(raop_txt)/sizeof(raop_txt[0]));

    // Advertise _airplay._tcp for better discovery on modern iOS
    char mac_colons[18];
    snprintf(mac_colons, sizeof(mac_colons), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    mdns_txt_item_t airplay_txt[] = {
        {"txtvers", "1"},
        {"deviceid", mac_colons},
        {"features", "0x77"},
        {"model", "AirPortExpress1,1"},
        {"vv", "2"},
    };
    mdns_service_remove("_airplay", "_tcp");
    mdns_service_add(device_name, "_airplay", "_tcp", 5000, airplay_txt, sizeof(airplay_txt)/sizeof(airplay_txt[0]));

    ESP_LOGI(TAG, "RAOP service registered: %s._raop._tcp.local", raop_service_name);
}

void airplay_start(const char *device_name)
{
    strncpy(saved_device_name, device_name, sizeof(saved_device_name)-1);

    // 1. Start RTSP server FIRST
    rtsp_server_start(5000);

    // 2. Initialize mDNS
    esp_err_t err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "mdns_init failed: %d", err);
        return;
    }

    mdns_hostname_set(device_name);
    register_mdns_services(device_name);
}

void airplay_reannounce(void)
{
    ESP_LOGI(TAG, "Re-announcing RAOP service...");
    register_mdns_services(saved_device_name);
}
