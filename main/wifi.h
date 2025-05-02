#pragma once

#include <sys/cdefs.h>

// WiFi configuration info
typedef struct WiFiConfigInfo {
    char wifiSsid[64];      // SSID string
    char wifiPasswd[64];    // Password string
    uint8_t wifiMac[6];     // WiFi MAC address
    int8_t rssi;            // WiFi RSSI value [in dBm]
    uint8_t priChan;        // WiFi primary channel
    uint32_t wifiIpAddr;    // DHCP assigned IP address
    uint32_t wifiGwAddr;    // WiFi router IP address
} WiFiConfigInfo;

__BEGIN_DECLS

extern int wifiInit(WiFiConfigInfo *info);
extern int wifiSetCredentials(const char *ssid, const char *passwd);
extern int wifiConnect(void);
extern int wifiDisconnect(void);
extern int wifiEnable(bool enable);

__END_DECLS
