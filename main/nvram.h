#pragma once

#include "wifi.h"

// App configuration info
typedef struct AppConfigInfo {
    WiFiConfigInfo wifiConfigInfo;
    int8_t utcOffset;

} AppConfigInfo;


__BEGIN_DECLS

extern int nvramOpen(void);
extern int nvramClose(void);
extern int nvramRead(AppConfigInfo *configInfo);
extern int nvramWrite(const AppConfigInfo *configInfo);
extern int nvramClear(void);

__END_DECLS
