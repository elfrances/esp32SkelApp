#include "sdkconfig.h"

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

typedef enum WpsState {
    wpsIdle = 0,
    wpsInProg,
    wpsSuccess,
    wpsError,
} WpsState;

static WpsState wpsState = wpsIdle;

#ifdef CONFIG_WPS
static esp_wps_config_t wpsConfig = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);
static wifi_config_t wpsApCredentials[MAX_WPS_AP_CRED];
static int wpsRetries = 0;
#endif

static void wifiClearCredentials(AppData *appData)
{
    memset(appData->persData.wifiSsid, 0, sizeof (appData->persData.wifiSsid));
    memset(appData->persData.wifiPasswd, 0, sizeof (appData->persData.wifiPasswd));
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
    AppData *appData = arg;

    if (evtId == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *gotIp = evtData;
        appData->wifiIpAddr = gotIp->ip_info.ip.addr;
        appData->wifiGwAddr = gotIp->ip_info.gw.addr;
        char ipBuf[INET_ADDRSTRLEN];
        char gwBuf[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &appData->wifiIpAddr, ipBuf, sizeof (ipBuf));
        inet_ntop(AF_INET, &appData->wifiGwAddr, gwBuf, sizeof (gwBuf));
        mlog(info, "Connected to WiFi AP: ipAddr=%s gwAddr=%s mac=%s rssi=%s chan=%u", ipBuf, gwBuf, fmtLanMac(appData->wifiMac), fmtRssi(appData->wifiRssi), appData->wifiPriChan);

        // Set the LED solid blue to indicate we are
        // connected to the network.
        ledSet(on, blue);

#ifdef CONFIG_WPS
        wpsState = wpsIdle;
#endif

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
    AppData *appData = arg;
    static int connRetryCnt = 0;
    esp_err_t rc;

    if (evtId == WIFI_EVENT_STA_START) {
        //mlog(trace, "WIFI_EVENT_STA_START: wifiSsid=%s wifiPasswd=%s connRetryCnt=%d", confInfo->wifiSsid, confInfo->wifiPasswd, connRetryCnt);

        // Do we have valid credentials?
        if ((appData->persData.wifiSsid[0] != '\0') && (appData->persData.wifiPasswd[0] != '\0')) {
            wifi_config_t wifiConfig = {0};

            if (connRetryCnt == 0) {
                int passwdLen = strlen(appData->persData.wifiPasswd);
                char hiddenPasswd[passwdLen + 1];
                for (int i = 0; i < (passwdLen - 1); i++) {
                    hiddenPasswd[i] = '*';
                }
                hiddenPasswd[passwdLen - 1] = '\0';
                mlog(info, "Connecting using saved WiFi config: SSID=\"%s\" PASS=\"%s\" ...", appData->persData.wifiSsid, hiddenPasswd);
            }

            // Attempt to connect with the saved WiFi credentials
            memcpy(wifiConfig.sta.ssid, appData->persData.wifiSsid, sizeof (wifiConfig.sta.ssid));
            memcpy(wifiConfig.sta.password, appData->persData.wifiPasswd, sizeof (wifiConfig.sta.password));
            esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);
            if ((rc = esp_wifi_connect()) != 0) {
                mlog(error, "esp_wifi_connect: rc=0x%04x connState=%d", rc, wifiConnState);
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
            wpsState = wpsInProg;
            wpsRetries = 0;
            mlog(trace, "WPS started ...");
#endif
        }
    } else if (evtId == WIFI_EVENT_STA_STOP) {
        //mlog(trace, "WIFI_EVENT_STA_STOP");
#ifdef CONFIG_WPS
        if (wpsState == wpsInProg) {
            mlog(trace, "Disabling WPS ...");
            esp_wifi_wps_disable();
            wpsState = wpsIdle;
        }
#endif
    } else if (evtId == WIFI_EVENT_STA_CONNECTED) {
        int rssi = 0;
        wifi_second_chan_t secChan = 0;
        uint8_t priChan = 0;

        // Get the connection info
        esp_wifi_sta_get_rssi(&rssi);
        esp_wifi_get_channel(&priChan, &secChan);
        esp_wifi_get_mac(ESP_IF_WIFI_STA, appData->wifiMac);
        appData->wifiRssi = rssi;
        appData->wifiPriChan = priChan;
        //mlog(trace, "Connected to WiFi AP: rssi=%s, priChan=%u, mac=%s", fmtRssi(appData->wifiRssi), appData->wifiPriChan, fmtLanMac(appData->wifiMac));
        wifiConnState = wifiConnected;
        connRetryCnt = 0;
    } else if (evtId == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = evtData;
        const uint8_t reason = disconn->reason;
        bool retry = true;

        //mlog(trace, "WIFI_EVENT_STA_DISCONNECTED: reason=%u", reason);
        if ((appData->persData.wifiSsid[0] != '\0') && (appData->persData.wifiPasswd[0] != '\0')) {
            if (connRetryCnt++ >= 3) {
                // The saved WiFi credentials don't seem to work anymore,
                // likely because the AP's SSID/Password has changed or
                // the ESP32 device was moved to a different WiFi network.
                mlog(warning, "Saved WiFi credentials are invalid !");
                wifiClearCredentials(appData);
                connRetryCnt = 0;
            } else if (reason == WIFI_REASON_NO_AP_FOUND) {
                // The saved SSID is not available
                mlog(warning, "Saved SSID \"%s\" not available: connRetryCnt=%u", appData->persData.wifiSsid, connRetryCnt);
            } else if ((reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) ||
                       (reason == WIFI_REASON_AUTH_FAIL)) {
                // The saved password didn't work... Notice that after
                // a reset we may get one "bogus" AUTH_FAIL response from
                // the WiFi AP (not clear why) so we allow for a few
                // retries before giving up and switching to WPS.
                if (disconn->reason != WIFI_REASON_AUTH_FAIL) {
                    mlog(warning, "Saved PASS \"%s\" didn't work: connRetryCnt=%u", appData->persData.wifiPasswd, connRetryCnt);
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
                connRetryCnt = 0;
                retry = false;
#ifdef CONFIG_WPS
            } else if (((wpsState == wpsInProg) || (wpsState == wpsSuccess)) && (reason == WIFI_REASON_ASSOC_LEAVE)) {
                // When we are using WPS to obtain the WiFi credentials we
                // expect to get disconnected with reason=ASSOC_LEAVE, so
                // we can ignore this event...
#endif
            } else {
                mlog(warning, "Disconnected from WiFi AP: reason=%u connState=%d wpsState=%d", reason, wifiConnState, wpsState);
            }
        } else {
            LogLevel logLevel = warning;
#ifdef CONFIG_WPS
            // When using WPS to autoconfig the WiFi credentials, we may
            // get an ASSOC_LEAVE, AUTH_FAIL and 802_1X_AUTH_FAILED that
            // we can ignore, as the WPS procedure seems to complete OK
            // anyway...
            if ((reason == WIFI_REASON_ASSOC_LEAVE) ||
                (reason == WIFI_REASON_AUTH_FAIL) ||
                (reason == WIFI_REASON_802_1X_AUTH_FAILED)) {
                logLevel = trace;
                retry = false;
            }
#endif
            mlog(logLevel, "Disconnected from WiFi AP: reason=%u connState=%d wpsState=%d", reason, wifiConnState, wpsState);
        }

        wifiConnState = wifiDisconnected;

        if (retry) {
            // Let's try again...
            esp_wifi_stop();
            esp_wifi_start();
        }
#ifdef CONFIG_WPS
    } else if (evtId == WIFI_EVENT_STA_WPS_ER_SUCCESS) {
        wifi_event_sta_wps_er_success_t *evt = (wifi_event_sta_wps_er_success_t *) evtData;
        wifi_config_t staConfig = {0};

        mlog(trace, "WIFI_EVENT_STA_WPS_ER_SUCCESS");
        if (evt == NULL) {
            // The WiFi AP returned a single set of credentials
            // and the ESP-IDF WPS API already configured them
            // so there is no need to call esp_wifi_set_config().
        } else {
            // The WiFi AP returned multiple sets of credentials
            for (int i = 0; i < evt->ap_cred_cnt; i++) {
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

        // Save the WiFi credentials we got via WPS
        if (wifiSetCredentials(appData, (char *) staConfig.sta.ssid, (char *) staConfig.sta.password) != 0) {
            mlog(error, "Failed to save WPS credentials !");
        }

        // Don't need WPS anymore
        esp_wifi_wps_disable();
        wpsState = wpsSuccess;
    } else if (evtId == WIFI_EVENT_STA_WPS_ER_FAILED) {
        mlog(error, "WIFI_EVENT_STA_WPS_ER_FAILED");
    } else if (evtId == WIFI_EVENT_STA_WPS_ER_TIMEOUT) {
        // The WPS process timed out!
    	if (++wpsRetries < CONFIG_WPS_RETRIES) {
			mlog(warning, "WPS timeout: restarting WPS to try again ...");
			wpsState = wpsIdle;
			esp_wifi_wps_disable();
			esp_wifi_wps_enable(&wpsConfig);
			esp_wifi_wps_start(0);
			wpsState = wpsInProg;
    	} else {
    		mlog(warning, "Giving up on WPS!");
    		vTaskResume(callingTaskHandle);
    	}
    } else if (evtId == WIFI_EVENT_STA_WPS_ER_PIN) {
        //mlog(trace, "WIFI_EVENT_STA_WPS_ER_PIN");
    } else if (evtId == WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP) {
        mlog(warning, "WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP");
#endif  // CONFIG_WPS
    } else {
        mlog(warning, "Unhandled WiFi event: evtId=%" PRId32 "", evtId);
    }
}

int wifiSetCredentials(AppData *appData, const char *ssid, const char *passwd)
{
    size_t ssidLen, passLen;

    // Check the length of the SSID and PASS strings
    if ((ssidLen = strlen(ssid)) > sizeof (appData->persData.wifiSsid)) {
        mlog(error, "SSID=\"%s\" is too long!", ssid);
        return -1;
    }
    if ((passLen = strlen(passwd)) > sizeof (appData->persData.wifiPasswd)) {
        mlog(error, "Password=\"%s\" is too long!", passwd);
        return -1;
    }

    wifiClearCredentials(appData);
    memcpy(appData->persData.wifiSsid, ssid, ssidLen);
    memcpy(appData->persData.wifiPasswd, passwd, passLen);

    return 0;
}

int wifiInit(AppData *appData)
{
    if (esp_netif_create_default_wifi_sta() == NULL)
        return -1;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    if (esp_wifi_init(&cfg) != ESP_OK)
        return -1;

    // Install WiFi event handler
    if (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEvtHandler, appData) != ESP_OK)
        return -1;

    // Install IP config event handler
    if (esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ipEvtHandler, appData) != ESP_OK)
        return -1;

    // Set WiFi mode to "station"
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
        return -1;

    // To override the WiFi credentials stored
    // in NVRAM, uncomment the following lines.
    //strcpy(appData->persData.wifiSsid, "HomeSweetHome");
    //strcpy(appData->persData.wifiPasswd, "T0PSeaKret!");

    return 0;
}

int wifiConnect(AppData *appData)
{
    esp_err_t rc = 0;

    if (wifiConnState == wifiDisconnected) {
        mlog(info, "Connecting to WiFi AP ...");

        // Clear the current IP addresses
        appData->wifiIpAddr = 0;
        appData->wifiGwAddr = 0;

        // Make the LED blue and blink 4x per second to
        // indicate that we are connecting to the WiFi
        // network.
        ledSet(blink4, blue);

        // Let's get this party going!
        if ((rc = esp_wifi_start()) != ESP_OK) {
            mlog(error, "esp_wifi_start: rc=0x%04x", rc);
            return -1;
        }

        // Block until the connection attempt finishes
        callingTaskHandle = xTaskGetCurrentTaskHandle();
        vTaskSuspend(NULL);
    } else {
        mlog(warning, "Connection request ignored: connState=%d", wifiConnState);
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

int wifiEnable(AppData *appData, bool enable)
{
    mlog(info, "%sabling WiFi ...", (enable) ? "En" : "Dis");

    if (enable && (wifiConnState != wifiConnected)) {
        return wifiConnect(appData);
    } else if (!enable) {
        wifiConnState = wifiDisconnecting;
        esp_wifi_stop();
    }

    return 0;
}

#endif  // CONFIG_WIFI_STATION
