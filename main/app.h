#pragma once

#include <sys/cdefs.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp32.h"

// App's persistent data
typedef struct AppPersData {
    char wifiSsid[64];      // SSID string
    char wifiPasswd[64];    // Password string
    int8_t utcOffset;       // UTC offset (in hours)

    // Custom app persistent data goes below

} AppPersData;

// App's data
typedef struct AppData {
    AppPersData persData;   // saved to (restored from) NVM
    const char *buildType;
    const esp_app_desc_t *appDesc;
    struct timeval baseTime;
    TickType_t baseTicks;
#ifdef CONFIG_WIFI_STATION
    uint8_t wifiMac[6];     // WiFi MAC address
    int8_t wifiRssi;        // WiFi RSSI value [in dBm]
    uint8_t wifiPriChan;    // WiFi primary channel
    uint32_t wifiIpAddr;    // DHCP assigned IP address
    uint32_t wifiGwAddr;    // WiFi router IP address
#endif

    // Custom app data goes below

} AppData;

// Path for the mlog.txt file
extern const char *mlogFilePath;

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
