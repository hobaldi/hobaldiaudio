#include "bt_a2dp.h"
#include "i2s_out.h"
#include "audio_engine.h"

void app_main(void) {
    audio_engine_init(NULL);
    i2s_out_init();
    bt_a2dp_init();
}
