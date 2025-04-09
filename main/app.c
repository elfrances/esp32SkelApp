#include "app.h"
#include "esp32.h"
#include "mlog.h"

// This is the app's main task
void appMainTask(void *parms)
{
    while (true) {
        mlog(trace, "Hello world!");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}
