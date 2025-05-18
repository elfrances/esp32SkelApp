#pragma once

#include <sys/cdefs.h>

#include "app.h"

// WiFi connection info
typedef struct WiFiConnInfo {
    uint8_t wifiMac[6];     // WiFi MAC address
    int8_t rssi;            // WiFi RSSI value [in dBm]
    uint8_t priChan;        // WiFi primary channel
    uint32_t wifiIpAddr;    // DHCP assigned IP address (in network byte order)
    uint32_t wifiGwAddr;    // WiFi router IP address (in network byte order)
} WiFiConnInfo;

__BEGIN_DECLS

extern int wifiInit(AppData *appData);
extern int wifiSetCredentials(AppData *appData, const char *ssid, const char *passwd);
extern int wifiConnect(AppData *appData);
extern int wifiDisconnect(void);
extern int wifiEnable(AppData *appData, bool enable);

__END_DECLS
