idf_component_register(
    SRCS "main.c" "wifi.c"
    REQUIRES
        bluetooth_a2dp
        audio_engine
        i2s_out
)

