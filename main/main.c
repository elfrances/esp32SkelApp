#include "app.h"
#include "ble.h"
#include "esp32.h"
#include "led.h"
#include "mlog.h"
#include "nvram.h"
#include "wifi.h"

AppConfigInfo appConfigInfo;

#ifdef CONFIG_WIFI_STATION
// Configure SNTP and get the current date and time
static int sntpInit(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // pool.ntp.org is a large cluster of NTP servers
    // distributed all over the world...
    esp_sntp_setservername(0, "pool.ntp.org");

    esp_sntp_init();

    // Wait up to 10 sec to get the current ToD
    for (int i = 0; i < 100; i++) {
        usleep(100000); // 100 ms
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            // Done!
            mlog(info, "Date and time set!");
            return 0;
        }
    }

    return -1;
}
#endif  // CONFIG_WIFI_STATION

void app_main(void)
{
    const esp_app_desc_t *appDesc = esp_app_get_description();
#if (CONFIG_COMPILER_OPTIMIZATION_DEFAULT)
    const char *buildType = "Debug";
#else
    const char *buildType = "Release";
#endif
    esp_err_t err;

    printf("Firmware %s (%s) built on %s at %s using ESP-IDF %s\n",
            appDesc->version, buildType, appDesc->date, appDesc->time, appDesc->idf_ver);


#if 1
    // On the ESP32-S3 DevKit the USB port drops whenever
    // the board is reset. This delay allows the user to
    // reconnect the terminal emulator to the UART port in
    // order to see the early initialization log messages.
    vTaskDelay(pdMS_TO_TICKS(5000));
#endif

    // Initialize the message logging API. Notice that at
    // this point NTP has not set the correct date & time
    // yet, therefore the timestamps will be based on the
    // Epoch (1970-01-01 at 00:00:00) ...
    if (msgLogInit(false) != 0) {
        printf("SPONG! Failed to init msgLog API!\n");
        return;
    }

    // Initialize the LED API
    if (ledInit() != 0) {
        mlog(fatal, "ledInit");
    }

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

    if (wifiConnect(&appConfigInfo.wifiConfigInfo) != 0) {
        mlog(fatal, "wifiInit!");
    }

    // Now that we are connected to the network, set
    // the date and time.
    if (sntpInit() != 0) {
        mlog(warning, "Failed to set date and time!");
    }

    // Save the WiFi credentials
    if (nvramWrite(&appConfigInfo) != 0) {
        mlog(fatal, "Can't save app's config info!");
    }
#endif

#ifdef CONFIG_APP_MAIN_TASK
    // Spawn the appMain task that will do all the work
    if (xTaskCreatePinnedToCore(appMainTask, "appMain", CONFIG_MAIN_TASK_STACK, NULL, CONFIG_MAIN_TASK_PRIO, NULL, tskNO_AFFINITY) != pdPASS) {
        mlog(fatal, "Can't spawn appMain task!");
    }
#else
    // Add your app code here...
#endif

}
