#pragma once

#include <sys/cdefs.h>

#include "app.h"

__BEGIN_DECLS

extern int wifiInit(AppData *appData);
extern int wifiSetCredentials(AppData *appData, const char *ssid, const char *passwd);
extern int wifiConnect(AppData *appData);
extern int wifiDisconnect(void);
extern int wifiEnable(AppData *appData, bool enable);

__END_DECLS
