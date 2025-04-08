#include "esp32.h"
#include "led.h"
#include "mlog.h"
#include "nvram.h"

AppConfigInfo appConfigInfo;

void app_main(void)
{
    esp_err_t err;

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

}
