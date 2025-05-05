#pragma once

#include <sys/cdefs.h>
#include <stdint.h>

#include "esp32.h"
#include "wifi.h"

// App configuration info
typedef struct AppConfigInfo {
    WiFiConfigInfo wifiConfigInfo;
    int8_t utcOffset;

} AppConfigInfo;

// This is the app's configuration info
extern AppConfigInfo appConfigInfo;

// Path for the mlog.txt file
extern const char *mlogFilePath;

// Base (reference) time and ticks
extern struct timeval baseTime;
extern TickType_t baseTicks;

typedef struct SerialNumber {
    uint8_t digits[4];
} SerialNumber;

__BEGIN_DECLS

extern void appMainTask(void *parms);
extern void getSerialNumber(SerialNumber *sn);
extern int restartDevice(void);
extern int clearConfig(void);
extern int dumpMlogFile(void);

__END_DECLS
