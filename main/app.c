#include "app.h"
#include "esp32.h"
#include "led.h"
#include "mlog.h"
#include "nvram.h"

// This is the app's configuration info
AppConfigInfo appConfigInfo;

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
// App initialization
static int appInit(void)
{
    // TBD
    return 0;
}

// This is the app's main task. It runs an infinite work
// loop, keeping a constant wake up interval.
void appMainTask(void *parms)
{
    const TickType_t wakeupPeriodTicks = pdMS_TO_TICKS(CONFIG_MAIN_TASK_WAKEUP_PERIOD);

    // Initialize whatever is needed before entering
    // the infinite work loop.
    if (appInit() != 0) {
        mlog(fatal, "App initialization failed!");
    }

    while (true) {
        TickType_t startTicks, elapsedTicks;

        // Record the start of this new pass of our
        // work loop.
        startTicks = xTaskGetTickCount();

        // Custom app code goes here...
        mlog(trace, "Hello world!");

        // Figure out how much time we spent so far
        elapsedTicks = xTaskGetTickCount() - startTicks;

        if (elapsedTicks < wakeupPeriodTicks) {
            // Sleep until the next poll period...
            TickType_t delayTicks = wakeupPeriodTicks - elapsedTicks;
            vTaskDelay(delayTicks);
        } else if (elapsedTicks > wakeupPeriodTicks) {
            // Oops! We exceeded the required wake up period!
            mlog(warning, "Wakeup period exceeded by %lu ms !!!", pdTICKS_TO_MS(elapsedTicks - wakeupPeriodTicks));
        }
    }

    vTaskDelete(NULL);
}
#endif
