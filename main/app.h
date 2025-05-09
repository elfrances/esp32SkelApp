#pragma once

#include <sys/cdefs.h>
#include <stdint.h>

#include "esp32.h"
#include "wifi.h"

// App configuration info
typedef struct AppConfigInfo {
    WiFiConfigInfo wifiConfigInfo;
    int8_t utcOffset;

    // Custom app config goes here

} AppConfigInfo;

// App's configuration info record
extern AppConfigInfo appConfigInfo;

// App build info
typedef struct AppBuildInfo {
    const char *buildType;
    const esp_app_desc_t *appDesc;
} AppBuildInfo;

// App's build info record
extern AppBuildInfo appBuildInfo;

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
extern int dumpMlogFile(bool warn);
extern int deleteMlogFile(bool warn);

__END_DECLS
