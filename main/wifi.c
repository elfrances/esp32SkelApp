#include "esp32.h"
#include "fgc.h"
#include "led.h"
#include "mlog.h"
#include "wifi.h"

#ifdef CONFIG_WIFI_STATION

static TaskHandle_t callingTaskHandle = NULL;

typedef enum WifiConnState {
    wifiDisconnected = 0,
    wifiConnecting,
    wifiConnected,
    wifiDisconnecting,
} WifiConnState;

static WifiConnState wifiConnState = wifiDisconnected;

#ifdef CONFIG_WPS
static esp_wps_config_t wpsConfig = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);
static wifi_config_t wpsApCredentials[MAX_WPS_AP_CRED];
static int numApCred = 0;
#endif

static void wifiClearCredentials(WiFiConfigInfo *confInfo)
{
    memset(confInfo->wifiSsid, 0, sizeof (confInfo->wifiSsid));
    memset(confInfo->wifiPasswd, 0, sizeof (confInfo->wifiPasswd));
}

// Format a LAN MAC address
const char *fmtLanMac(const uint8_t *addr)
{
    static char buf[20];
    snprintf(buf, sizeof (buf), "%02X:%02X:%02X:%02X:%02X:%02X",
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return buf;
}

// Format an RSSI value
const char *fmtRssi(int8_t rssi)
{
    static char fmtBuf[32];
    const char *textColor;

    if (rssi >= -70) {
        // -70 to 0 dBm: Excellent
        textColor = GREEN_FGC;
    } else if (rssi >= -80) {
        // -80 to -71 dBm: Good
        textColor = YELLOW_FGC;
    } else if (rssi >= -90) {
        // -90 to -81 dBm: Fair
        textColor = ORANGE_FGC;
    } else {
        // Bellow -90 dBm: Weak
        textColor = RED_FGC;
    }

    snprintf(fmtBuf, sizeof (fmtBuf), "%s%d dBm%s", textColor, rssi, RESET_FGC);

    return fmtBuf;
}

// NOTE: this handler runs in the context of the "sys_evt" task
static void ipEvtHandler(void *arg, esp_event_base_t evtBase, int32_t evtId, void *evtData)
{
    WiFiConfigInfo *confInfo = arg;

    if (evtId == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *gotIp = evtData;
        confInfo->wifiIpAddr = gotIp->ip_info.ip.addr;
        confInfo->wifiGwAddr = gotIp->ip_info.gw.addr;
        char ipBuf[INET_ADDRSTRLEN];
        char gwBuf[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &confInfo->wifiIpAddr, ipBuf, sizeof (ipBuf));
        inet_ntop(AF_INET, &confInfo->wifiGwAddr, gwBuf, sizeof (gwBuf));
        mlog(info, "Connected to router: ipAddr=%s gwAddr=%s", ipBuf, gwBuf);

        // Set the LED solid blue to indicate we are
        // connected to the network.
        ledSet(on, blue);

        // Wake up the task that triggered the WiFi
        // connection...
        vTaskResume(callingTaskHandle);
    } else {
        mlog(warning, "Unhandled event: id=%" PRId32 "", evtId);
    }
}

// NOTE: this handler runs in the context of the "sys_evt" task
static void wifiEvtHandler(void *arg, esp_event_base_t evtBase, int32_t evtId, void *evtData)
{
    WiFiConfigInfo *confInfo = arg;
    static int connRetryCnt = 0;
    esp_err_t rc;

    if (evtId == WIFI_EVENT_STA_START) {
        //mlog(trace, "WIFI_EVENT_STA_START: magic=0x%08" PRIx32 " wifiSsid=%s wifiPasswd=%s connRetryCnt=%d", confInfo->magic, confInfo->wifiSsid, confInfo->wifiPasswd, connRetryCnt);

        // Do we have valid credentials?
        if ((confInfo->wifiSsid[0] != '\0') && (confInfo->wifiPasswd[0] != '\0')) {
            wifi_config_t wifiConfig = {0};

            if (connRetryCnt == 0) {
                mlog(info, "Connecting using saved WiFi config: SSID=\"%s\" PASS=\"%s\" ...", confInfo->wifiSsid, confInfo->wifiPasswd);
            }

            // Attempt to connect with the saved WiFi credentials
            memcpy(wifiConfig.sta.ssid, confInfo->wifiSsid, sizeof (wifiConfig.sta.ssid));
            memcpy(wifiConfig.sta.password, confInfo->wifiPasswd, sizeof (wifiConfig.sta.password));
            esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);
            if ((rc = esp_wifi_connect()) != 0) {
                mlog(error, "esp_wifi_connect: rc=0x%04x", rc);
            }
        } else {
            // Make the LED magenta and blink 4x per second to
            // indicate that the saved credentials didn't work
            // and we need user intervention to set them either
            // via WPS, or manually using the BLE Companion app.
            ledSet(blink4, magenta);
#ifdef CONFIG_WPS
            mlog(info, "Enabling WPS to get the WiFi credentials ...");
            esp_wifi_wps_enable(&wpsConfig);
            esp_wifi_wps_start(0);
#endif
        }
    } else if (evtId == WIFI_EVENT_STA_STOP) {
        //mlog(trace, "WIFI_EVENT_STA_STOP");
#ifdef CONFIG_WPS
        esp_wifi_wps_disable();
#endif
    } else if (evtId == WIFI_EVENT_STA_CONNECTED) {
        int rssi = 0;
        wifi_second_chan_t secChan = 0;
        uint8_t priChan = 0;

        //mlog(trace, "WIFI_EVENT_STA_CONNECTED");
        wifiConnState = wifiConnected;
        connRetryCnt = 0;

        esp_wifi_sta_get_rssi(&rssi);
        esp_wifi_get_channel(&priChan, &secChan);
        esp_wifi_get_mac(ESP_IF_WIFI_STA, confInfo->wifiMac);
        mlog(info, "Connected to WiFi AP: rssi=%s, priChan=%u, mac=%s", fmtRssi(rssi), priChan, fmtLanMac(confInfo->wifiMac));
    } else if (evtId == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = evtData;
        const uint8_t reason = disconn->reason;
        bool retry = true;

        //mlog(trace, "WIFI_EVENT_STA_DISCONNECTED: magic=0x%08" PRIx32 " reason=%u", server->appConfInfo.magic, reason);
        wifiConnState = wifiDisconnected;
        if ((confInfo->wifiSsid[0] != '\0') && (confInfo->wifiPasswd[0] != '\0')) {
            if (connRetryCnt++ >= 3) {
                // The saved WiFi credentials don't seem to work anymore,
                // likely because the AP's SSID/Password has changed or
                // the ESP32 device was moved to a different WiFi network.
                mlog(warning, "Saved WiFi credentials are invalid !");
                wifiClearCredentials(confInfo);
                connRetryCnt = 0;
            } else if (reason == WIFI_REASON_NO_AP_FOUND) {
                // The saved SSID is not available
                mlog(warning, "Saved SSID \"%s\" not available: connRetryCnt=%u", confInfo->wifiSsid, connRetryCnt);
            } else if ((reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) ||
                       (reason == WIFI_REASON_AUTH_FAIL)) {
                // The saved password didn't work... Notice that after
                // a reset we may get one "bogus" AUTH_FAIL response from
                // the WiFi AP (not clear why) so we allow for a few
                // retries before giving up and switching to WPS.
                if (disconn->reason != WIFI_REASON_AUTH_FAIL) {
                    mlog(warning, "Saved PASS \"%s\" didn't work: connRetryCnt=%u", confInfo->wifiPasswd, connRetryCnt);
                }
#ifdef CONFIG_WPS
            } else if (reason == WIFI_REASON_802_1X_AUTH_FAILED) {
                // When using WPS to autoconfig the WiFi credentials we
                // seem to get this error on the first connection
                // attempt...
                mlog(warning, "802_1X_AUTH failed: connRetryCnt=%u", connRetryCnt);
#endif
            } else if (reason == WIFI_REASON_BEACON_TIMEOUT) {
                mlog(warning, "Connection to WiFi AP dropped !");
            } else if (wifiConnState == wifiDisconnecting) {
                mlog(info, "Successfully disconnected from WiFi AP !");
                wifiConnState = wifiDisconnected;
                connRetryCnt = 0;
                retry = false;
            } else {
                //mlog(warning, "Disconnected from WiFi AP: reason=%u", reason);
            }
        } else {
            mlog(warning, "Disconnected from WiFi AP: reason=%u", reason);
#ifdef CONFIG_WPS
            // When using WPS to autoconfig the WiFi credentials, we may
            // get an AUTH_FAIL and 802_1X_AUTH_FAILED that we can ignore
            // as the WPS procedure seems to complete OK anyway...
            if ((reason == WIFI_REASON_AUTH_FAIL) ||
                (reason == WIFI_REASON_802_1X_AUTH_FAILED)) {
                retry = false;
            }
#endif
        }

        if (retry) {
            // Let's try again...
            esp_wifi_stop();
            esp_wifi_start();
        }
#ifdef CONFIG_WPS
    } else if (evtId == WIFI_EVENT_STA_WPS_ER_SUCCESS) {
        wifi_event_sta_wps_er_success_t *evt = (wifi_event_sta_wps_er_success_t *) evtData;
        wifi_config_t staConfig = {0};

        //mlog(trace, "WIFI_EVENT_STA_WPS_ER_SUCCESS");
        if (evt == NULL) {
            // The WiFi AP returned a single set of credentials
            // and the ESP-IDF WPS API already configured them
            // so there is no need to call esp_wifi_set_config().
        } else {
            // The WiFi AP returned multiple sets of credentials
            numApCred = evt->ap_cred_cnt;
            for (int i = 0; i < numApCred; i++) {
                memcpy(wpsApCredentials[i].sta.ssid, evt->ap_cred[i].ssid, MAX_SSID_LEN);
                memcpy(wpsApCredentials[i].sta.password, evt->ap_cred[i].passphrase, MAX_PASSPHRASE_LEN);
            }
            if ((rc = esp_wifi_set_config(WIFI_IF_STA, &wpsApCredentials[0])) != 0) {
                mlog(error, "esp_wifi_set_config: rc=0x%04x", rc);
            }
        }

        // Get the WPS credentials returned by the WiFi AP
        if ((rc = esp_wifi_get_config(WIFI_IF_STA, &staConfig)) != 0) {
            mlog(error, "esp_wifi_get_config: rc=0x%04x", rc);
        }

        mlog(info, "Got WPS credentials: SSID=\"%s\" PASS=\"%s\"", (char *) staConfig.sta.ssid, (char *) staConfig.sta.password);

        // Don't need WPS anymore
        esp_wifi_wps_disable();

        // Save the WiFi credentials we got via WPS
        if (wifiSetCredentials(confInfo, (char *) staConfig.sta.ssid, (char *) staConfig.sta.password) == 0) {
            // Try to connect with the WPS credentials...
            esp_wifi_stop();
            esp_wifi_start();
        } else {
            mlog(error, "Failed to save WPS credentials !");
        }
    } else if (evtId == WIFI_EVENT_STA_WPS_ER_FAILED) {
        mlog(error, "WIFI_EVENT_STA_WPS_ER_FAILED");
    } else if (evtId == WIFI_EVENT_STA_WPS_ER_TIMEOUT) {
        mlog(warning, "WPS timeout: restarting WPS to try again ...");
        // Restart the WPS process...
        esp_wifi_wps_disable();
        esp_wifi_wps_enable(&wpsConfig);
        esp_wifi_wps_start(0);
    } else if (evtId == WIFI_EVENT_STA_WPS_ER_PIN) {
        //mlog(trace, "WIFI_EVENT_STA_WPS_ER_PIN");
    } else if (evtId == WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP) {
        mlog(warning, "WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP");
#endif  // CONFIG_WPS
    } else {
        mlog(warning, "Unhandled WiFi event: evtId=%" PRId32 "", evtId);
    }
}

int wifiSetCredentials(WiFiConfigInfo *confInfo, const char *ssid, const char *passwd)
{
    size_t ssidLen, passLen;

    // Check the length of the SSID and PASS strings
    if ((ssidLen = strlen(ssid)) > sizeof (confInfo->wifiSsid)) {
        mlog(error, "SSID=\"%s\" is too long!", ssid);
        return -1;
    }
    if ((passLen = strlen(passwd)) > sizeof (confInfo->wifiPasswd)) {
        mlog(error, "Password=\"%s\" is too long!", passwd);
        return -1;
    }

    wifiClearCredentials(confInfo);
    memcpy(confInfo->wifiSsid, ssid, ssidLen);
    memcpy(confInfo->wifiPasswd, passwd, passLen);

    return 0;
}

int wifiInit(WiFiConfigInfo *confInfo)
{
    if (esp_netif_create_default_wifi_sta() == NULL)
        return -1;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Install WiFi event handler
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEvtHandler, confInfo);

    // Install IP config event handler
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ipEvtHandler, confInfo);

    // Set WiFi mode to "station"
    esp_wifi_set_mode(WIFI_MODE_STA);

    // To override the WiFi credentials stored
    // in NVRAM, uncomment the following lines.
    //strcpy(confInfo->wifiSsid, "ElkAndMoose");
    //strcpy(confInfo->wifiPasswd, "sunvalley1228");

    return 0;
}

int wifiConnect(WiFiConfigInfo *confInfo)
{
    esp_err_t rc = 0;

    mlog(info, "Connecting to router over WiFi ...");

    if (wifiConnState == wifiDisconnected) {
        // Clear the current IP addresses
        confInfo->wifiIpAddr = 0;
        confInfo->wifiGwAddr = 0;

        // Let's get this party going!
        if ((rc = esp_wifi_start()) != ESP_OK) {
            mlog(error, "esp_wifi_start: rc=0x%04x", rc);
            return -1;
        }

        // Block until the connection attempt finishes
        callingTaskHandle = xTaskGetHandle(pcTaskGetName(NULL));
        vTaskSuspend(callingTaskHandle);
    }

    return 0;
}

int wifiDisconnect(void)
{
    esp_err_t rc = 0;

    if (wifiConnState == wifiConnected) {
        mlog(info, "Disconnecting from WiFi AP ...");

        wifiConnState = wifiDisconnecting;

        if ((rc = esp_wifi_disconnect()) != ESP_OK) {
            mlog(error, "esp_wifi_disconnect: rc=0x%04x", rc);
            return -1;
        }
    }

    return 0;
}

#endif  // CONFIG_WIFI_STATION
