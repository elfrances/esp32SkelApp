#pragma once

#include <sys/cdefs.h>

#include "app.h"

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
    coNoOp = 0x00,
    coRestartDevice,
    coClearConfig,
    coStartOtaUpdate,
    coSetLogLevel,      // {UINT8: 0=NONE, 1=INFO, 2=TRACE, 3=DEBUG}
    coSetLogDest,       // {UINT8: 0=Console, 1=File, 2=Both}
    coSetUtcTime,       // {UINT32: # seconds since the Epoch, INT8: # hours east or west from GMT}
    coSetUtcOffset,     // {INT8: # hours east or west from GMT}
    coSetWiFiState,     // {UINT8: 0=Disabled, 1=Enabled}
    coDumpMlogFile,
    coDeleteMlogFile,
} CmdOpCode;

// Command Request
typedef struct CmdReq {
    uint8_t opCode;
    uint8_t params[0];
} CmdReq;

// Command Status Code
typedef enum CmdStatusCode {
    csIdle = 0x00,
    csInProg,
    csSuccess,
    csFailed,
    csInvOpCode,
    csInvParam,
} CmdStatusCode;

// Command Status
typedef struct CmdStatus {
    uint8_t opCode;
    uint8_t status;
} CmdStatus;

__BEGIN_DECLS

extern int bleInit(AppData *appData);

extern void blePutUINT16(uint8_t *data, uint16_t value);
extern void blePutUINT24(uint8_t *data, uint32_t value);
extern void blePutUINT32(uint8_t *data, uint32_t value);
extern void blePutUINT64(uint8_t *data, uint64_t value);
extern uint16_t bleGetUINT16(const uint8_t *data);
extern uint32_t bleGetUINT24(const uint8_t *data);
extern uint32_t bleGetUINT32(const uint8_t *data);
extern uint64_t bleGetUINT64(const uint8_t *data);

__END_DECLS
