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
// App Info
typedef struct AppInfo {
    TaskHandle_t taskHandle;
#if CONFIG_APP_MAIN_TASK_WAKEUP_METHOD_ESP_TIMER
    esp_timer_handle_t wakeupTimerHandle;
#endif
#ifdef CONFIG_MAIN_TASK_TIME_WORK_LOOP
    uint64_t workLoopCount;
    uint64_t sumWorkLoopTime;
    uint64_t minWorkLoopTime;
    uint64_t maxWorkLoopTime;
    uint64_t avgWorkLoopTime;
#endif
} AppInfo;

// Custom app initialization
static int appCustInit(AppInfo *appInfo)
{
    // TBD
    return 0;
}

// This is the app's main task. It runs an infinite work
// loop, trying its best to keep a constant wake up interval.
#if CONFIG_APP_MAIN_TASK_WAKEUP_METHOD_TASK_DELAY
void appMainTask(void *parms)
{
    AppInfo appInfo = {0};
    const TickType_t wakeupPeriodTicks = pdMS_TO_TICKS(CONFIG_MAIN_TASK_WAKEUP_PERIOD);

    appInfo.taskHandle = xTaskGetCurrentTaskHandle();

    // Do any custom app initialization before
    // entering the infinite work loop.
    if (appCustInit(&appInfo) != 0) {
        mlog(fatal, "Custom app initialization failed!");
    }

    while (true) {
        TickType_t startTicks, elapsedTicks;

        // Record the start of this new pass of our
        // work loop.
        startTicks = xTaskGetTickCount();

        // Custom app code goes here...
        {
            mlog(info, "TICK!");
        }

        // Figure out how much time we spent so far
        elapsedTicks = xTaskGetTickCount() - startTicks;

        if (elapsedTicks < wakeupPeriodTicks) {
            // Sleep until the next poll period...
            TickType_t delayTicks = wakeupPeriodTicks - elapsedTicks;
            vTaskDelay(delayTicks);
        } else if (elapsedTicks > wakeupPeriodTicks) {
            // Oops! We exceeded the required wake up period!
            mlog(warning, "%u ms wakeup period exceeded by %lu ms !!!", CONFIG_MAIN_TASK_WAKEUP_PERIOD, pdTICKS_TO_MS(elapsedTicks - wakeupPeriodTicks));
        }
    }

    vTaskDelete(NULL);
}
#else
// Wake up the appMainTask. This function runs
// in the context of the ESP Timer task.
static void wakeupTimerCb(void *arg)
{
    const TaskHandle_t appMainHandle = *(TaskHandle_t *) arg;
    xTaskNotifyGive(appMainHandle);
}

void appMainTask(void *parms)
{
    AppInfo appInfo = {0};
    const uint64_t wakeupPeriod = CONFIG_MAIN_TASK_WAKEUP_PERIOD * 1000;    // in usec

    appInfo.taskHandle = xTaskGetCurrentTaskHandle();

    // Create the ESP Timer used to post the
    // wake up events.
    {
        esp_timer_create_args_t wakeupTimerArgs = {0};
        wakeupTimerArgs.callback = wakeupTimerCb;
        wakeupTimerArgs.arg = &appInfo.taskHandle;
        wakeupTimerArgs.dispatch_method = ESP_TIMER_TASK;
        wakeupTimerArgs.name = "wakeupTmr";
        if (esp_timer_create(&wakeupTimerArgs, &appInfo.wakeupTimerHandle) != ESP_OK) {
            mlog(fatal, "Failed to create wakeupTimer!");
        }
    }

    // Do any custom app initialization before
    // entering the infinite work loop.
    if (appCustInit(&appInfo) != 0) {
        mlog(fatal, "Custom app initialization failed!");
    }

    while (true) {
        uint64_t startTime, elapsedTime;

        // Record the start of this new pass of our
        // work loop.
        startTime = esp_timer_get_time();

        // Custom app code goes here...
        {
            mlog(info, "TICK!");
        }

        // Figure out how much time we spent so far
        elapsedTime = esp_timer_get_time() - startTime;

        if (elapsedTime < wakeupPeriod) {
            // Set up the wake up alarm ...
            uint64_t sleepTime = wakeupPeriod - elapsedTime;
            if (esp_timer_start_once(appInfo.wakeupTimerHandle, sleepTime) != ESP_OK) {
                mlog(fatal, "Failed to start wakeupTimer!");
            }

            // ... and go to sleep
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        } else if (elapsedTime > wakeupPeriod) {
            // Oops! We exceeded the required wake up period!
            uint32_t exceedTime = (elapsedTime - wakeupPeriod);
            mlog(warning, "%u ms wakeup period exceeded by %lu.%03lu ms !!!", CONFIG_MAIN_TASK_WAKEUP_PERIOD, (exceedTime / 1000), (exceedTime % 1000));
        }

#ifdef CONFIG_MAIN_TASK_TIME_WORK_LOOP
        // Update the work loop timing stats
        {
            uint64_t wakeupTime = esp_timer_get_time();
            uint64_t workLoopTime = wakeupTime - startTime;
            if (workLoopTime < appInfo.minWorkLoopTime) {
                appInfo.minWorkLoopTime = workLoopTime;
            } else if (workLoopTime > appInfo.maxWorkLoopTime) {
                appInfo.maxWorkLoopTime = workLoopTime;
            }
            appInfo.sumWorkLoopTime += workLoopTime;
            appInfo.avgWorkLoopTime = appInfo.sumWorkLoopTime / ++appInfo.workLoopCount;
            if ((appInfo.workLoopCount % (1000 / CONFIG_MAIN_TASK_WAKEUP_PERIOD)) == 0) {
                mlog(trace, "Work Loop Time Stats: min=%llu avg=%llu max=%llu", appInfo.minWorkLoopTime, appInfo.avgWorkLoopTime, appInfo.maxWorkLoopTime);
            }
        }
#endif
    }

    vTaskDelete(NULL);
}
#endif
#endif  // CONFIG_APP_MAIN_TASK
