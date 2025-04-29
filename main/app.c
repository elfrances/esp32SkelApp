#include "app.h"
#include "esp32.h"
#include "mlog.h"

void getSerialNumber(SerialNumber *sn)
{
    uint8_t macAddr[6];
    esp_read_mac(macAddr, ESP_MAC_BASE);
    memcpy(sn->digits, (macAddr+2), 4);
}

#ifdef CONFIG_APP_MAIN_TASK
// This is the app's main task
void appMainTask(void *parms)
{
    while (true) {
        TickType_t startTicks, elapsedTicks;
        const TickType_t loopPeriodTicks = pdMS_TO_TICKS(CONFIG_MAIN_TASK_TICK_PERIOD);

        startTicks = xTaskGetTickCount();

        mlog(trace, "Hello world!");

        elapsedTicks = xTaskGetTickCount() - startTicks;

        if (elapsedTicks < loopPeriodTicks) {
            // Sleep until the next poll period...
            TickType_t delayTicks = loopPeriodTicks - elapsedTicks;
            vTaskDelay(delayTicks);
        }
    }

    vTaskDelete(NULL);
}
#endif
