idf_component_register(
    SRCS "main.c" "wifi.c"
    REQUIRES
        audio_engine
        i2s_out
        wifi_pcm
        airplay
        esp_http_server
        nvs_flash
)

