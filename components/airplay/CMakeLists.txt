idf_component_register(
    SRCS "airplay.c" "rtsp_server.c" "raop_rtp.c" "alac.c"
    INCLUDE_DIRS "."
    REQUIRES mdns audio_engine lwip
)

target_link_libraries(${COMPONENT_LIB} PRIVATE "-lm")
