#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bt_a2dp.h"
#include "audio_engine.h"

void app_main(void) {
    bt_a2dp_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

