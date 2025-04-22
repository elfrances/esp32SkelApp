#pragma once

#include <sys/cdefs.h>
#include <stdint.h>

#include "wifi.h"

// App configuration info
typedef struct AppConfigInfo {
    WiFiConfigInfo wifiConfigInfo;
    int8_t utcOffset;

} AppConfigInfo;

extern AppConfigInfo appConfigInfo;

typedef struct SerialNumber {
    uint8_t digits[4];
} SerialNumber;

__BEGIN_DECLS

extern void appMainTask(void *parms);
extern void getSerialNumber(SerialNumber *sn);

__END_DECLS
