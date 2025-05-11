#include "app.h"
#include "esp32.h"
#include "led.h"
#include "mlog.h"
#include "nvram.h"

// App's configuration info record
AppConfigInfo appConfigInfo;

// App's build info record
AppBuildInfo appBuildInfo;

#ifdef CONFIG_FAT_FS
// Path for the MLOG.TXT file
const char *mlogFilePath = CONFIG_FAT_FS_MOUNT_POINT "/MLOG.TXT";
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

int restartDevice(void)
{
    mlog(info, "Restarting the device...");

    // Turn off the LED
    ledSet(off, black);

#ifdef CONFIG_WIFI_STATION
    // Disconnect from the WiFi AP
    wifiDisconnect();
    vTaskDelay(pdMS_TO_TICKS(250));
#endif

    // Disable message logging
    msgLogSetLevel(none);

    // THE END
    esp_restart();

    return 0;
}

int clearConfig(void)
{
    mlog(info, "Clearing the device configuration...");
    return nvramClear();
}

int dumpMlogFile(bool warn)
{
#ifdef CONFIG_FAT_FS
    FILE *fp;

    if ((fp = fopen(mlogFilePath, "r")) != NULL) {
        char lineBuf[CONFIG_MSG_LOG_MAX_LEN];
        int n = 0;

        printf("\n### Dump of MLOG.TXT ###\n");

        while (fgets(lineBuf, sizeof (lineBuf), fp) != NULL) {
            printf("MLOG: %s", lineBuf);
            if (++n == 100) {
                // This delay is to prevent the task
                // watchdog to expire...
                vTaskDelay(1);
                n = 0;
            }
        }

        printf("### End of dump ###\n\n");

        fclose(fp);
    } else if (errno == ENOENT) {
        // This is not necessarily an error. Warn the
        // user only if asked by the caller...
        if (warn) {
            mlog(info, "%s not available!", mlogFilePath);
        }
    } else {
        mlog(errNo, "Failed to open %s!", mlogFilePath);
        return -1;
    }
#endif

    return 0;
}

int deleteMlogFile(bool warn)
{
#ifdef CONFIG_FAT_FS
    if (unlink(mlogFilePath) != 0) {
        if (errno == ENOENT) {
            // This is not necessarily an error. Warn the
            // user only if asked by the caller...
            if (warn) {
                mlog(info, "%s not available!", mlogFilePath);
            }
        } else {
            mlog(errNo, "Failed to delete %s!", mlogFilePath);
            return -1;
        }
    }
#endif

    return 0;
}

#ifdef CONFIG_APP_MAIN_TASK
// Wake up the appMainTask
static void wakeupTimerCb(void *arg)
{
    const TaskHandle_t appMainHandle = *(TaskHandle_t *) arg;
    xTaskNotifyGive(appMainHandle);
}

// Custom app initialization
static int appCustInit(void)
{
    // TBD
    return 0;
}

// This is the app's main task. It runs an infinite work
// loop, trying its best to keep a constant wake up interval.
void appMainTask(void *parms)
{
    const TaskHandle_t appMainHandle = xTaskGetCurrentTaskHandle();
    const uint64_t wakeupPeriod = CONFIG_MAIN_TASK_WAKEUP_PERIOD * 1000;    // in usec
    esp_timer_create_args_t wakeupTimerArgs = {0};
    esp_timer_handle_t wakeupTimerHandle;

    // Create the ESP Timer used to post the
    // wake up events.
    wakeupTimerArgs.callback = wakeupTimerCb;
    wakeupTimerArgs.arg = (void *) &appMainHandle;
    wakeupTimerArgs.dispatch_method = ESP_TIMER_ISR;
    wakeupTimerArgs.name = "wakeupTmr";
    if (esp_timer_create(&wakeupTimerArgs, &wakeupTimerHandle) != ESP_OK) {
        mlog(fatal, "Failed to create wakeupTimer!");
    }

    // Do any custom app initialization before
    // entering the infinite work loop.
    if (appCustInit() != 0) {
        mlog(fatal, "Custom app initialization failed!");
    }

    while (true) {
        uint64_t startTime, elapsedTime;

        // Record the start of this new pass of our
        // work loop.
        startTime = esp_timer_get_time();

        // Custom app code goes here...
        mlog(trace, "Hello world!");

        // Figure out how much time we spent so far
        elapsedTime = esp_timer_get_time() - startTime;

        if (elapsedTime < wakeupPeriod) {
            // Sleep until the next poll period...
            uint64_t sleepTime = wakeupPeriod - elapsedTime;
            if (esp_timer_start_once(wakeupTimerHandle, sleepTime) != ESP_OK) {
                mlog(fatal, "Failed to start wakeupTimer!");
            }
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        } else if (elapsedTime > wakeupPeriod) {
            // Oops! We exceeded the required wake up period!
            uint32_t exceedTime = (elapsedTime - wakeupPeriod);
            mlog(warning, "Wakeup period exceeded by %lu.%06lu ms !!!", (exceedTime / 1000), (exceedTime % 1000));
        }
    }

    vTaskDelete(NULL);
}
#endif
