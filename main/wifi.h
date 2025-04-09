#pragma once

#include <sys/cdefs.h>

// WiFi configuration info
typedef struct WiFiConfigInfo {
    char wifiSsid[64];
    char wifiPasswd[64];
} WiFiConfigInfo;

__BEGIN_DECLS

extern int wifiInit(WiFiConfigInfo *info);
extern int wifiSetCredentials(WiFiConfigInfo *confInfo, const char *ssid, const char *passwd);

__END_DECLS
