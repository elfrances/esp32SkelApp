#include "app.h"
#include "esp32.h"
#include "mlog.h"

// This is the app's configuration info
AppConfigInfo appConfigInfo;

#ifdef CONFIG_FAT_FS
// Mount path for the FAT Flash File System
const char *fatFsMountPath = "/fatfs";
#endif

// Base (reference) time and ticks
struct timeval baseTime;
TickType_t baseTicks;

void getSerialNumber(SerialNumber *sn)
{
    uint8_t macAddr[6];
    esp_read_mac(macAddr, ESP_MAC_BASE);
    memcpy(sn->digits, (macAddr+2), 4);
}

#ifdef CONFIG_APP_MAIN_TASK
// This is the app's main task. It runs an infinite work
// loop, keeping a constant wakeup interval.
void appMainTask(void *parms)
{
    const TickType_t wakeupPeriodTicks = pdMS_TO_TICKS(CONFIG_MAIN_TASK_WAKEUP_PERIOD);

    while (true) {
        TickType_t startTicks, elapsedTicks;

        startTicks = xTaskGetTickCount();

        mlog(trace, "Hello world!");

        elapsedTicks = xTaskGetTickCount() - startTicks;

        if (elapsedTicks < wakeupPeriodTicks) {
            // Sleep until the next poll period...
            TickType_t delayTicks = wakeupPeriodTicks - elapsedTicks;
            vTaskDelay(delayTicks);
        }
    }

    vTaskDelete(NULL);
}
#endif
