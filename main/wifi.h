#pragma once

#include <sys/cdefs.h>

// WiFi configuration info
typedef struct WiFiConfigInfo {
    char wifiSsid[64];
    char wifiPasswd[64];
    uint8_t wifiMac[6];
} WiFiConfigInfo;

__BEGIN_DECLS

extern int wifiInit(WiFiConfigInfo *info);
extern int wifiSetCredentials(WiFiConfigInfo *confInfo, const char *ssid, const char *passwd);
extern int wifiConnect(WiFiConfigInfo *confInfo);
extern int wifiDisconnect(void);

__END_DECLS
