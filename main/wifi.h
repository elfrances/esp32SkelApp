#pragma once

#include <sys/cdefs.h>

// WiFi configuration info
typedef struct WiFiConfigInfo {
    char wifiSsid[64];      // SSID string
    char wifiPasswd[64];    // Password string
    uint8_t wifiMac[6];     // WiFi MAC address
    uint32_t wifiIpAddr;    // DHCP assigned IP address
    uint32_t wifiGwAddr;    // WiFi router IP address
} WiFiConfigInfo;

__BEGIN_DECLS

extern int wifiInit(WiFiConfigInfo *info);
extern int wifiSetCredentials(WiFiConfigInfo *confInfo, const char *ssid, const char *passwd);
extern int wifiConnect(WiFiConfigInfo *confInfo);
extern int wifiDisconnect(void);

__END_DECLS
