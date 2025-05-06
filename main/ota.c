#include "sdkconfig.h"

#ifdef CONFIG_OTA_UPDATE

#include <stdio.h>

#include "esp32.h"
#include "led.h"
#include "mlog.h"
#include "ota.h"

// Delay (in ms) before the system auto-resets after
// a successful OTA firmware update.
#define POST_UPDATE_RESET_DELAY 5000

// Delay (in ms) before the system auto-resets after
// a failed OTA firmware update.
#define FAIL_UPDATE_RESET_DELAY 3000

// This is the combined length of the binary file headers
#define FILE_HEADERS_LEN    (sizeof (esp_image_header_t) + sizeof (esp_image_segment_header_t) + sizeof (esp_app_desc_t))

// This is the URL for the latest firmware image released
static char otaUpdateUrl[80];

// Custom OTA API function to terminate an OTA update
extern esp_err_t esp_https_ota_terminate(void);

// This is the server's certificate
extern const uint8_t server_cert_pem_start[] asm("_binary_ota_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ota_cert_pem_end");

typedef enum OtaUpdState {
    otaUpdStart = 0,
    otaUpdConnected,
    otaUpdTerminating,
    otaUpdTerminated,
    otaUpdFinished,
} OtaUpdState;

static int httpErrno;
static OtaUpdState otaUpdState;
static bool versionChecked;

static int checkFirmwareVersions(const uint8_t *dataBuf, int dataLen)
{
    if (dataLen >= FILE_HEADERS_LEN) {
        const esp_partition_t *runPart = esp_ota_get_running_partition();
        esp_app_desc_t runAppDesc, updAppDesc;

        // Get the description of the running firmware
        esp_ota_get_partition_description(runPart, &runAppDesc);

        // Get the description of the firmware on the server
        memcpy(&updAppDesc, &dataBuf[FILE_HEADERS_LEN - sizeof (esp_app_desc_t)], sizeof (updAppDesc));

        if (strcmp(runAppDesc.version, updAppDesc.version) == 0) {
            // We are already running the latest firmware so
            // there is no need to proceed with the update.
            mlog(info, "The firmware is up to date!");
            return -1;
        } else {
            mlog(info, "Updating firmware %s -> %s ...", runAppDesc.version, updAppDesc.version);
        }

        versionChecked = true;
    }

    return 0;
}

static esp_err_t httpEvtHandler(esp_http_client_event_t *evt)
{
    esp_err_t err;
    static int onDataCount = 0;
    static int dataLenSoFar = 0;

    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        //mlog(trace, "HTTP_EVENT_ERROR");
        httpErrno = errno;
        break;

    case HTTP_EVENT_ON_CONNECTED:
        mlog(info, "Connected to the OTA update server ...");
        otaUpdState = otaUpdConnected;
        break;

    case HTTP_EVENT_HEADER_SENT:
        //mlog(trace, "HTTP_EVENT_HEADER_SENT");
        break;

    case HTTP_EVENT_ON_HEADER:
        //mlog(trace, "HTTP_EVENT_ON_HEADER: key=%s, value=%s", evt->header_key, evt->header_value);
        onDataCount = 0;
        dataLenSoFar = 0;
        break;

    case HTTP_EVENT_ON_DATA:
        //mlog(trace, "HTTP_EVENT_ON_DATA: data=%p len=%d soFar=%d", evt->data, evt->data_len, dataLenSoFar);
        if (evt->data != NULL) {
            onDataCount++;
            dataLenSoFar += evt->data_len;
            if ((onDataCount % 10) == 0) {
                // Update the file download "progress bar" ...
                esp_rom_printf("#");
            }
            if (!versionChecked) {
                if (checkFirmwareVersions(evt->data, evt->data_len) != 0) {
                    // Abort the OTA update...
                    mlog(info, "Aborting OTA update ...");
                    otaUpdState = otaUpdTerminating;
                    if ((err = esp_https_ota_terminate()) != ESP_OK) {
                        mlog(fatal, "Failed to terminate OTA update: err=%d", err);
                    }
                }
            }
        } else {
            esp_rom_printf("\n");
            mlog(info, "Download finished: fileSize=%d", dataLenSoFar);
        }
        break;

    case HTTP_EVENT_ON_FINISH:
        //mlog(trace, "HTTP_EVENT_ON_FINISH: soFar=%d", dataLenSoFar);
        break;

    case HTTP_EVENT_DISCONNECTED:
        //mlog(trace, "HTTP_EVENT_DISCONNECTED: otaUpdState=%u", otaUpdState);
        if (otaUpdState == otaUpdTerminating) {
            otaUpdState = otaUpdTerminated;
        }
        break;

    case HTTP_EVENT_REDIRECT:
        //mlog(trace, "HTTP_EVENT_REDIRECT");
        break;

    default:
        //mlog(trace, "evtId=%d", evt->event_id);
        break;
    }

    return ESP_OK;
}

static void otaUpdTask(void *arg)
{
    const char *tgtDevSuffix = "";
    bool autoRestart = false;
    LedMode ledMode = on;
    LedColor ledColor = cyan;
    TickType_t delayTicks = 0;
    esp_err_t err;

    // Build the OTA update URL: "https://<server-addr>:<port>/<idf-tgt>/update.bin"
    // where: <idf-tgt>={esp32|esp32c3|esp32c3-zero|esp32s3|esp32s3-zero}

#if ((CONFIG_RGB_LED_GPIO == 10) || (CONFIG_RGB_LED_GPIO == 21))
    // This is a Waveshare C3-Zero or S3-Zero device
    tgtDevSuffix = "-zero";
#endif

    snprintf(otaUpdateUrl, sizeof (otaUpdateUrl), "https://%s:%u/%s%s/update.bin", CONFIG_OTA_SERVER_FQDN, CONFIG_OTA_TCP_PORT, CONFIG_IDF_TARGET, tgtDevSuffix);

    // Disable WiFi power-saving mode to speed up the
    // firmware download...
    esp_wifi_set_ps(WIFI_PS_NONE);

    esp_http_client_config_t config = {
        .url = otaUpdateUrl,
        .cert_pem = (char *) server_cert_pem_start,
        .event_handler = httpEvtHandler,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t otaConfig = {
        .http_config = &config,
    };

    // Make the LED blink cyan 4X per second to indicate
    // the OTA firmware update is in progress...
    ledSet(blink4, cyan);

    mlog(info, "Starting OTA firmware update from %s", config.url);

    httpErrno = 0;
    otaUpdState = otaUpdStart;
    versionChecked = false;

    if ((err = esp_https_ota(&otaConfig)) == ESP_OK) {
        mlog(info, "OTA firmware update succeeded.");
        autoRestart = true;
        delayTicks = pdMS_TO_TICKS(POST_UPDATE_RESET_DELAY);
    } else if (otaUpdState == otaUpdTerminated) {
        mlog(info, "OTA update terminated.");
    } else {
        mlog(error, "%s", (otaUpdState < otaUpdConnected) ? "Unable to connect to OTA update server." : "OTA update failed!");
        ledMode = blink4;
        ledColor = red;
        delayTicks = pdMS_TO_TICKS(FAIL_UPDATE_RESET_DELAY);
    }

    // Give the user a visual indication of the result
    // of the firmware update.
    ledSet(ledMode, ledColor);
    vTaskDelay(delayTicks);
    ledSet(off, black);

    if (autoRestart) {
        // Restart the device to activate the new firmware
        esp_restart();
    }

    vTaskDelete(NULL);
}

int otaUpdateStart(void)
{
    if (xTaskCreatePinnedToCore(otaUpdTask, "otaUpd", CONFIG_OTA_TASK_STACK, NULL, CONFIG_OTA_TASK_PRIO, NULL, tskNO_AFFINITY) != pdPASS) {
        mlog(error, "Failed to start otaUpdTask!");
        return -1;
    }

    return 0;
}

#endif  // CONFIG_OTA
