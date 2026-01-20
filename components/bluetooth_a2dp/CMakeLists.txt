idf_component_register(
    SRCS
        bt_a2dp.c
    INCLUDE_DIRS
        .
    REQUIRES
        bt
        nvs_flash
        audio_engine
)

