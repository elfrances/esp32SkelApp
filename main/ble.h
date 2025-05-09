#pragma once

#include <sys/cdefs.h>

// The following definitions are the external API to be
// used by apps that want to connect to SkelApp over BLE.

// Custom Device Config Service
#define GATT_DEVICE_CONFIG_SERVICE_UUID     CONFIG_DEVICE_CONFIG_SERVICE_UUID
#define GATT_DCS_WIFI_CREDENTIALS_UUID          (CONFIG_DEVICE_CONFIG_SERVICE_UUID+1)   // READ, WRITE
#define GATT_DCS_OPERATING_STATUS_UUID          (CONFIG_DEVICE_CONFIG_SERVICE_UUID+2)   // READ
#define GATT_DCS_COMMAND_REQUEST_UUID           (CONFIG_DEVICE_CONFIG_SERVICE_UUID+3)   // READ, WRITE, INDICATE
#define GATT_DCS_COMMAND_HELP_UUID              (CONFIG_DEVICE_CONFIG_SERVICE_UUID+4)   // READ

// Device Operating Status: defines the format of the
// data returned when reading the DCS_OPERATING_STATUS
// characteristic.
typedef struct DevOperStatus {
    uint8_t sysUpTime[4];       // UINT32: System Up Time [in seconds]
    uint8_t wifiStaIpAddr[4];   // UINT32: WiFi Station IPv4 Address
    uint8_t wifiApIpAddr[4];    // UINT32: WiFi Access Point IPv4 Address
    uint8_t wifiStaMacAddr[6];  // UINT8[6]: WiFi Station MAC Address
    uint8_t wifiRssi;           // INT8: WiFi RSSI
    uint8_t wifiChan;           // UINT8: WiFi Channel
    uint8_t blePerMacAddr[6];   // UINT8[6]: BLE Peripheral MAC Address
    uint8_t bleCenMacAddr[6];   // UINT8[6]: BLE Central MAC Address
    uint8_t freeHeapMem[2];     // UINT16: Free Heap Memory [in KB]
    uint8_t maxHeapMemBlk[2];   // UINT16: Max Heap Memory Block [in KB]
    uint8_t freeFatFsSpace[2];  // UINT16: Free FAT FS Space [in kB]
} DevOperStatus;

// Command Op Code
typedef enum CmdOpCode {
    coNoOp = 0,
    coRestartDevice,
    coClearConfig,
    coStartOtaUpdate,
    coSetLogLevel,      // {UINT8: 0=NONE, 1=INFO, 2=TRACE, 3=DEBUG}
    coSetLogDest,       // {UINT8: 0=Console, 1=File, 2=Both}
    coSetUtcTime,       // {UINT32: # seconds since the Epoch}
    coSetUtcOffset,     // {INT8: # hours east or west from GMT}
    coSetWiFiState,     // {UINT8: 0=Disabled, 1=Enabled}
    coDumpMlogFile,
    coDeleteMlogFile,
} CmdOpCode;

// Command Status Code
typedef enum CmdStatus {
    csIdle = 0,
    csInProg,
    csSuccess,
    csFailed,
    csInvOpCode,
    csInvParam,
} CmdStatus;

__BEGIN_DECLS

extern int bleInit(void);

__END_DECLS
