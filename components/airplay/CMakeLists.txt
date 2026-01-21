idf_component_register(
    SRCS "airplay.c" "rtsp_server.c"
    INCLUDE_DIRS "."
    REQUIRES mdns audio_engine lwip
)
