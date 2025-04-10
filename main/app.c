#include "app.h"
#include "esp32.h"
#include "mlog.h"

void getSerialNumber(SerialNumber *sn)
{
    uint8_t macAddr[6];
    esp_read_mac(macAddr, ESP_MAC_BASE);
    memcpy(sn->digits, (macAddr+2), 4);
}

// This is the app's main task
void appMainTask(void *parms)
{
    while (true) {
        mlog(trace, "Hello world!");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}
