#include <assert.h>
#include <dirent.h>
#include <stdlib.h>

#include "sdkconfig.h"

#include "app.h"
#include "ble.h"
#include "esp32.h"
#include "fgc.h"
#include "https.h"
#include "led.h"
#include "mlog.h"
#include "nvram.h"
#include "timeval.h"
#include "wifi.h"

// Check the consistency of some critical SkelApp
// sdkconfig settings.
#ifdef CONFIG_RGB_LED
_Static_assert((CONFIG_RGB_LED_TASK_PRIO <= (configMAX_PRIORITIES - 1)), "RGB_LED_TASK_PRIO is inconsistent with configMAX_PRIORITIES !");
#endif
#ifdef CONFIG_BLE_PERIPHERAL
_Static_assert((CONFIG_BLE_HOST_TASK_PRIO <= (configMAX_PRIORITIES - 1)), "BLE_HOST_TASK_PRIO is inconsistent with configMAX_PRIORITIES !");
#endif
#ifdef CONFIG_WEB_SERVER
_Static_assert((CONFIG_WEB_SERVER_TASK_PRIO <= (configMAX_PRIORITIES - 1)), "WEB_SERVER_TASK_PRIO is inconsistent with configMAX_PRIORITIES !");
#endif
#ifdef CONFIG_OTA_UPDATE
_Static_assert((CONFIG_OTA_TASK_PRIO <= (configMAX_PRIORITIES - 1)), "OTA_TASK_PRIO is inconsistent with configMAX_PRIORITIES !");
#endif
#ifdef CONFIG_APP_MAIN_TASK
_Static_assert((CONFIG_MAIN_TASK_PRIO <= (configMAX_PRIORITIES - 1)), "MAIN_TASK_PRIO is inconsistent with configMAX_PRIORITIES !");
#if CONFIG_APP_MAIN_TASK_WAKEUP_METHOD_TASK_DELAY
_Static_assert((CONFIG_MAIN_TASK_WAKEUP_PERIOD >= (1000 / CONFIG_FREERTOS_HZ)), "MAIN_TASK_WAKEUP_PERIOD is lower than the RTOS tick period !");
_Static_assert((CONFIG_MAIN_TASK_WAKEUP_PERIOD % (1000 / CONFIG_FREERTOS_HZ) == 0), "MAIN_TASK_WAKEUP_PERIOD is not a multiple of the RTOS tick period !");
#endif
#endif

#if CONFIG_WIFI_NTP
// Configure SNTP and get the current date and time
static int sntpInit(void)
{
    struct timeval now;

    gettimeofday(&now, NULL);

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, CONFIG_WIFI_NTP_SERVER);
    esp_sntp_init();

    // Wait up to 10 sec to get the current ToD
    for (int i = 0; i < 100; i++) {
        usleep(100000); // 100 ms
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            // Done! Now update the baseTime value used to
            // generate relative timestamps.
            struct timeval newNow;
            gettimeofday(&newNow, NULL);
            tvSub(&baseTime, &newNow, &now);
            mlog(info, "Date and time set!");
            return 0;
        }
    }

    return -1;
}
#endif  // CONFIG_WIFI_NTP

#ifdef CONFIG_FAT_FS
// Handle of the Wear Leveling API
static wl_handle_t wlHandle = WL_INVALID_HANDLE;

static int fatFsInit(void)
{
    const esp_vfs_fat_mount_config_t fatFsMountConfig = {
        .max_files = CONFIG_FAT_FS_MAX_FILES,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };
    esp_err_t err;
    uint64_t totalBytes, freeBytes, pctUsed;
    char fileName[272];

    if ((err = esp_vfs_fat_spiflash_mount_rw_wl(CONFIG_FAT_FS_MOUNT_POINT, "storage", &fatFsMountConfig, &wlHandle)) != ESP_OK) {
        mlog(error, "Failed to mount FATFS: %s", esp_err_to_name(err));
        return -1;
    }

    if ((err = esp_vfs_fat_info(CONFIG_FAT_FS_MOUNT_POINT, &totalBytes, &freeBytes)) != ESP_OK) {
        mlog(error, "Failed to get FATFS info: %s", esp_err_to_name(err));
        return -1;
    }
    pctUsed = ((totalBytes - freeBytes) * 100 * 100) / totalBytes;

    mlog(info, "FATFS: Size=%u Free=%u Used=%u.%02u%%", (unsigned) totalBytes, (unsigned) freeBytes, (unsigned) (pctUsed / 100), (unsigned) (pctUsed % 100));

    if (CONFIG_FAT_FS_LIST_FILES) {
        DIR *dir;
        struct dirent *dirEnt;
        size_t totalBytes = 0;

        // List the contents of the FATFS
        if ((dir = opendir(CONFIG_FAT_FS_MOUNT_POINT)) == NULL) {
            mlog(errNo, "Failed to open directory");
            return -1;
        }

        printf("\n\nDirectory contents for %s:\n\n", CONFIG_FAT_FS_MOUNT_POINT);
        printf("         Name     |   Size   |    Last Modified    \n");
        printf("    --------------+----------+---------------------\n");
        while ((dirEnt = readdir(dir)) != NULL) {
            if (dirEnt->d_type == DT_REG) {
                const char *dName = dirEnt->d_name;
                struct stat fileStat;
                struct tm brkDwnTime;
                char tsBuf[20]; // YYYY-MM-DD HH:MM:SS
                size_t fileSize = fileStat.st_size;

                snprintf(fileName, sizeof (fileName), "%s/%s", CONFIG_FAT_FS_MOUNT_POINT, dName);
                if (stat(fileName, &fileStat) != 0) {
                    mlog(errNo, "Failed to stat file %s", fileName);
                    return -1;
                }
                strftime(tsBuf, sizeof (tsBuf), "%Y-%m-%d %H:%M:%S", gmtime_r(&fileStat.st_mtim.tv_sec, &brkDwnTime));  // %H means 24-hour time
                printf("     %12s | %8zu | %19s \n", dName, fileSize, tsBuf);
                totalBytes += fileSize;
            }
        }
        printf("\nTotal bytes: %zu\n\n", totalBytes);

        closedir(dir);
    }

#ifdef CONFIG_MSG_LOG_DUMP
    // Dump the contents of the MLOG.TXT file
    dumpMlogFile(false);
#endif

    // Now that the FAT FS is mounted, see if we need to
    // re-set the log destination...
    if ((CONFIG_MSG_LOG_DEST == both) || (CONFIG_MSG_LOG_DEST == file)) {
        msgLogSetDest(CONFIG_MSG_LOG_DEST);
    }

    return 0;
}
#endif

// This function is called by the ESP-IDF "main" task during
// system start up.
void app_main(void)
{
    appBuildInfo.appDesc = esp_app_get_description();
#if (CONFIG_COMPILER_OPTIMIZATION_DEFAULT)
    appBuildInfo.buildType = "Debug";
#else
    appBuildInfo.buildType = "Release";
#endif
    esp_err_t err;

    // Reset the terminal's foreground color highlighting
    printf("%s\n", RESET_FGC);

#if 1
    // On the ESP32-S3 DevKit the USB port drops whenever
    // the board is reset. This delay allows the user to
    // reconnect the terminal emulator to the UART port in
    // order to see the early initialization log messages.
    printf("\n\n*** Forced 3 sec delay ***\n\n");
    vTaskDelay(pdMS_TO_TICKS(3000));
#endif

    printf("Firmware %s (%s) built on %s at %s using ESP-IDF %s\n",
            appBuildInfo.appDesc->version, appBuildInfo.buildType, appBuildInfo.appDesc->date, appBuildInfo.appDesc->time, appBuildInfo.appDesc->idf_ver);
    printf("System Info: CPU=%s CLK=%uMHz, FreeMem=%uKB MaxBlock=%uKB\n",
            CONFIG_IDF_TARGET, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
            heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024,
            heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT) / 1024);

    // Set the base time and ticks
    gettimeofday(&baseTime, NULL);
    baseTicks = xTaskGetTickCount();

#ifdef CONFIG_MSG_LOG
    // Initialize the message logging API. Please note:
    //
    // 1 - At this point NTP has not set the correct date
    // & time yet, therefore the ToD timestamps will be
    // based on the Epoch (1970-01-01 at 00:00:00)
    //
    // 2 - At this point we can only log to the console,
    // as the FAT FS is not initialized yet.
    if (msgLogInit(CONFIG_MSG_LOG_LEVEL, console) != 0) {
        printf("SPONG! Failed to init msgLog API!\n");
        return;
    }
#endif

#ifdef CONFIG_RGB_LED
    // Initialize the LED API
    if (ledInit() != 0) {
        mlog(fatal, "ledInit");
    }
#endif

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Initialize the TCP/IP network stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop handler
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Init NVRAM API
    if (nvramOpen() != 0) {
        mlog(fatal, "nvramOpen!");
    }

    // Read the app's config info from NVRAM
    if (nvramRead(&appConfigInfo) != 0) {
        mlog(fatal, "Can't read app's config info!");
    }

#ifdef CONFIG_BLE_PERIPHERAL
    if (bleInit() != 0) {
        mlog(fatal, "bleInit!");
    }
#endif

#ifdef CONFIG_WIFI_STATION
    // If we have hardwired WiFi credentials, use them...
    if ((strlen(CONFIG_WIFI_SSID) != 0) && (strlen(CONFIG_WIFI_PASSWD) != 0)) {
        strcpy(appConfigInfo.wifiConfigInfo.wifiSsid, CONFIG_WIFI_SSID);
        strcpy(appConfigInfo.wifiConfigInfo.wifiPasswd, CONFIG_WIFI_PASSWD);
    }

    if (wifiInit(&appConfigInfo.wifiConfigInfo) != 0) {
        mlog(fatal, "wifiInit!");
    }

    if (wifiConnect() != 0) {
        mlog(fatal, "wifiInit!");
    }

#if CONFIG_WIFI_NTP
    // Now that we are connected to the network, set
    // the current date and time.
    if (sntpInit() != 0) {
        mlog(warning, "Failed to set date and time!");
    }
#endif

    // Save the WiFi credentials
    if (nvramWrite(&appConfigInfo) != 0) {
        mlog(fatal, "Can't save app's config info!");
    }
#endif  // CONFIG_WIFI_STATION

#ifdef CONFIG_FAT_FS
    // Init the FAT FS
    if (fatFsInit() != 0) {
        mlog(fatal, "Failed to init FATFS!");
    }
#endif

#ifdef CONFIG_WEB_SERVER
    // Init the HTTP Web Server
    if (httpsInit() != 0) {
        mlog(fatal, "Failed to init HTTP Server!");
    }
#endif

#ifdef CONFIG_APP_MAIN_TASK
    // Spawn the appMain task that will do all the work
    if (xTaskCreatePinnedToCore(appMainTask, "appMain", CONFIG_MAIN_TASK_STACK, NULL, CONFIG_MAIN_TASK_PRIO, NULL, CONFIG_MAIN_TASK_CPU) != pdPASS) {
        mlog(fatal, "Can't spawn appMain task!");
    }
#else
    // Add your app code here...
#endif

}
