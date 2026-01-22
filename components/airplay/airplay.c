#include "airplay.h"
#include "mdns.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <string.h>

static const char *TAG = "airplay";

// Forward declaration for RTSP server
void rtsp_server_start(int port);

void airplay_start(const char *device_name)
{
    // 1. Initialize mDNS
    esp_err_t err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "mdns_init failed: %d", err);
        return;
    }

    mdns_hostname_set(device_name);
    mdns_instance_name_set(device_name);

    // RAOP (Remote Audio Output Protocol) advertisement
    // The instance name must be: [MACADDRESS]@[DeviceName]
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    // MAC without colons, lowercase
    char mac_str[13];
    snprintf(mac_str, sizeof(mac_str), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // MAC with colons, lowercase
    char mac_id[18];
    snprintf(mac_id, sizeof(mac_id), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char service_name[64];
    snprintf(service_name, sizeof(service_name), "%s@%s", mac_str, device_name);

    // TXT records for RAOP
    mdns_txt_item_t raop_txt[] = {
        {"ch", "2"},
        {"cn", "0,1,2,3"},
        {"da", "true"},
        {"et", "0,1"},
        {"md", "0,1,2"},
        {"pk", "b07727d6f6cd27efb57364aa71f99cf296c00350711963080033d59521360c70"},
        {"sf", "0x4"},
        {"tp", "UDP"},
        {"vn", "65537"},
        {"vs", "105.1"},
        {"am", "AppleTV1,1"},
    };
    mdns_service_add(service_name, "_raop", "_tcp", 5000, raop_txt, sizeof(raop_txt)/sizeof(raop_txt[0]));

    // TXT records for AirPlay
    mdns_txt_item_t airplay_txt[] = {
        {"features", "0x7"}, // 0x7 is standard for AirPlay 1 audio
        {"model", "AppleTV1,1"},
        {"deviceid", mac_id},
        {"vv", "2"},
    };
    // Note: Port 5000 is used for both as RTSP server handles both requests
    mdns_service_add(device_name, "_airplay", "_tcp", 5000, airplay_txt, sizeof(airplay_txt)/sizeof(airplay_txt[0]));

    ESP_LOGI(TAG, "AirPlay mDNS services registered as '%s'", device_name);

    // 2. Start RTSP server on port 5000
    rtsp_server_start(5000);
}
