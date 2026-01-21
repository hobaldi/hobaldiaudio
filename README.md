# Hobaldi

Hobaldi is a high-quality, lightweight, multi-platform audio streaming system.

 ESP-IDF `i2s_std` API.
   - Configured the audio output for CD quality (16-bit, 44.1kHz, Stereo) with GPIOs: BCLK=1, WS=2, DOUT=3.
   - Hardware Setup for PCM5102:

    BCLK: GPIO 1
    WS (LRCK): GPIO 2
    DOUT (DATA): GPIO 3
    GND: GND
    VCC: 3.3V


Goals:
- High sound quality
- Bluetooth audio
- Spotify Connect
- I2S DAC output
- ESP32 and Raspberry Pi support

Status: Early development.
