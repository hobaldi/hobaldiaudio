idf_component_register(
    SRCS "audio_engine.c" "buffer/audio_ringbuf.c"
    INCLUDE_DIRS "."
    REQUIRES i2s_out
)
