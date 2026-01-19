#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bt_a2dp.h"
#include "audio_engine.h"
#include "i2s_out.h"

void app_main(void) {
    audio_engine_init(NULL);
    i2s_out_init();
    bt_a2dp_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
