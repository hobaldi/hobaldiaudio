#include "airplay.h"
#include "mdns.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "airplay";

// Forward declaration for RTSP server
void rtsp_server_start(int port);

void airplay_start(const char *device_name)
{
    // 1. Initialize mDNS
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_init failed: %d", err);
        return;
    }

    mdns_hostname_set(device_name);
    mdns_instance_name_set(device_name);

    // RAOP (Remote Audio Output Protocol) advertisement
    // The service name should be: [MACADDRESS]@[DeviceName]
    // For now we use a dummy MAC or just the name
    char service_name[64];
    snprintf(service_name, sizeof(service_name), "112233445566@%s", device_name);

    mdns_service_add(service_name, "_raop", "_tcp", 5000, NULL, 0);

    // Add TXT records required for AirPlay
    mdns_service_txt_item_t raop_txt[] = {
        {"ch", "2"},          // channels
        {"cn", "0,1,2,3"},    // audio codecs
        {"da", "true"},       // digital audio
        {"et", "0,1"},        // encryption types (0: none, 1: RSA)
        {"md", "0,1,2"},      // metadata types
        {"pk", "b07727d6f6cd27efb57364aa71f99cf296c00350711963080033d59521360c70"}, // public key?
        {"sf", "0x4"},        // status flags
        {"tp", "UDP"},        // transport
        {"vn", "65537"},      // version
        {"vs", "105.1"},      // version string
        {"am", "AppleTV1,1"}, // model
    };
    mdns_service_txt_set("_raop", "_tcp", raop_txt, sizeof(raop_txt)/sizeof(raop_txt[0]));

    // Also advertise AirPlay service
    mdns_service_add(device_name, "_airplay", "_tcp", 7000, NULL, 0);
    mdns_service_txt_item_t airplay_txt[] = {
        {"features", "0x77"}, // features
        {"model", "AppleTV1,1"},
        {"deviceid", "11:22:33:44:55:66"},
    };
    mdns_service_txt_set("_airplay", "_tcp", airplay_txt, sizeof(airplay_txt)/sizeof(airplay_txt[0]));

    ESP_LOGI(TAG, "AirPlay mDNS services registered as '%s'", device_name);

    // 2. Start RTSP server on port 5000
    rtsp_server_start(5000);
}
