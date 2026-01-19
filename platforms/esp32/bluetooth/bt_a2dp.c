#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_a2dp_api.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#endif

#include "bt_a2dp.h"
#include "audio_engine.h"

static const char *TAG = "BT_A2DP";

/* A2DP audio data callback */
void bt_audio_data_cb(const uint8_t *data, uint32_t len) {
    /* 16-bit stereo PCM */
    audio_engine_write(data, len);
}

void bt_a2dp_init(void) {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_a2d_register_data_callback(bt_audio_data_cb));
    ESP_ERROR_CHECK(esp_a2d_sink_init());

    ESP_LOGI(TAG, "Bluetooth A2DP sink initialized");
}

