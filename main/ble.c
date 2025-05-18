#include "app.h"
#include "ble.h"
#include "esp32.h"
#include "led.h"
#include "mlog.h"
#include "nvram.h"
#include "ota.h"
#include "wifi.h"

#ifdef CONFIG_BLE_PERIPHERAL

// We need to make this pointer file-global because the
// NimBLE API doesn't support passing it as an argument.
static AppData *appData;

// Forward declarations
static void nimbleAdvertise(void);

void blePutUINT16(uint8_t *data, uint16_t value)
{
    *data++ = (value & 0xff);
    *data = ((value >> 8) & 0xff);
}

void blePutUINT24(uint8_t *data, uint32_t value)
{
    *data++ = (value & 0xff);
    *data++ = ((value >> 8) & 0xff);
    *data = ((value >> 16) & 0xff);
}

void blePutUINT32(uint8_t *data, uint32_t value)
{
    *data++ = (value & 0xff);
    *data++ = ((value >> 8) & 0xff);
    *data++ = ((value >> 16) & 0xff);
    *data = ((value >> 24) & 0xff);
}

void blePutUINT64(uint8_t *data, uint64_t value)
{
    *data++ = (value & 0xff);
    *data++ = ((value >> 8) & 0xff);
    *data++ = ((value >> 16) & 0xff);
    *data++ = ((value >> 24) & 0xff);
    *data++ = ((value >> 32) & 0xff);
    *data++ = ((value >> 40) & 0xff);
    *data++ = ((value >> 48) & 0xff);
    *data = ((value >> 56) & 0xff);
}

uint16_t bleGetUINT16(const uint8_t *data)
{
    uint16_t value = ((uint16_t) data[1] << 8) | (uint16_t) data[0];
    return value;
}

uint32_t bleGetUINT24(const uint8_t *data)
{
    uint32_t value = ((uint32_t) data[2] <<16) | ((uint32_t) data[1] << 8) | (uint32_t) data[0];
    return value;
}

uint32_t bleGetUINT32(const uint8_t *data)
{
    uint32_t value = ((uint32_t) data[3] << 24) | ((uint32_t) data[2] << 16) | ((uint32_t) data[1] << 8) | (uint32_t) data[0];
    return value;
}


// Inbound Connection Information
typedef struct InbConnInfo {
    time_t connTime;
    uint16_t connHandle;
    uint16_t cmdReqHandle;
    bool connEstablished;
    bool cmdReqIndicate;
    ble_addr_t ourAddr;
    ble_addr_t peerAddr;
} InbConnInfo;

static InbConnInfo inbConnInfo;

// Bluetooth SIG Device Info Service
#define GATT_DEVICE_INFO_SERVICE_UUID       0x180A
#define GATT_DIS_MANUFACTURER_NAME_UUID         0x2A29  // READ
#define GATT_DIS_MODEL_NUMBER_UUID              0x2A24  // READ
#define GATT_DIS_SERIAL_NUMBER_UUID             0x2A25  // READ
#define GATT_DIS_FIRMWARE_REVISION_UUID         0x2a26  // READ
#define GATT_DIS_HARDWARE_REVISION_UUID         0x2a27  // READ

static int deviceInfoCb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);
    char strBuf[64];
    const char *string = "???";

    if (uuid == GATT_DIS_MANUFACTURER_NAME_UUID) {
        string = CONFIG_MANUFACTURER_NAME;
    } else if (uuid == GATT_DIS_MODEL_NUMBER_UUID) {
        string = CONFIG_MODEL_NUMBER;
    } else if (uuid == GATT_DIS_SERIAL_NUMBER_UUID) {
        snprintf(strBuf, sizeof (strBuf), "%02X%02X%02X%02X",
                appData->serialNumber[0], appData->serialNumber[1], appData->serialNumber[2], appData->serialNumber[3]);
        string = strBuf;
    } else if (uuid == GATT_DIS_FIRMWARE_REVISION_UUID) {
        snprintf(strBuf, sizeof (strBuf), "%s (%s)", appData->appDesc->version, appData->buildType);
        string = strBuf;
    } else if (uuid == GATT_DIS_HARDWARE_REVISION_UUID) {
        string = CONFIG_IDF_TARGET;
    }

    //mlog(trace, "uuid=0x%04x value=%s", uuid, string);

    return (os_mbuf_append(ctxt->om, string, strlen(string)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// The WiFi Credentials data is a UTF-8 string that
// includes the SSID and Password strings concatenated
// using the character sequence "###" as a separator.
// For example, if SSID="ABCDE" and Password="12345"
// then the data read or written would be string:
// "ABCDE###12345".

static const size_t wifiCredMaxLen = sizeof (appData->persData.wifiSsid) + 3 + sizeof (appData->persData.wifiPasswd);

static int getWiFiCredentials(struct ble_gatt_access_ctxt *ctxt)
{
    char data[wifiCredMaxLen];
    uint16_t len = snprintf(data, sizeof (data), "%s###%s", appData->persData.wifiSsid, appData->persData.wifiPasswd);
    return (os_mbuf_append(ctxt->om, data, len) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int setWiFiCredentials(struct ble_gatt_access_ctxt *ctxt)
{
    struct os_mbuf *om = ctxt->om;
    char *data;
    uint16_t len;
    char *ssid, *pass, *sep;

    if ((om == NULL) || (om->om_data == NULL) || (om->om_len <= 2)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    data = (char *) om->om_data;
    len = om->om_len;
    if (len > wifiCredMaxLen) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    ssid = data;
    if ((sep = strstr(ssid, "###")) == NULL) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }
    *sep = '\0';    // null-terminate the SSID string

    pass = sep + 3; // Password string follows the separator
    len -= (strlen(ssid) + 3);
    pass[len] = '\0';   // null-terminate the Password string

    // Validate the individual string lengths
    if (strlen(ssid) >= sizeof (appData->persData.wifiSsid)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }
    if (strlen(pass) >= sizeof (appData->persData.wifiPasswd)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    strncpy(appData->persData.wifiSsid, ssid, sizeof (appData->persData.wifiSsid));
    strncpy(appData->persData.wifiPasswd, pass, sizeof (appData->persData.wifiPasswd));
    appData->persData.wifiPasswd[sizeof (appData->persData.wifiPasswd) - 1] = '\0';

    nvramWrite(&appData->persData);
    mlog(trace, "wifiSsid=%s wifiPasswd=%s", appData->persData.wifiSsid, appData->persData.wifiPasswd);

    return 0;
}

#ifdef CONFIG_DCS_OPERATING_STATUS_FMT_BINARY
static int getDevOperStatus(struct ble_gatt_access_ctxt *ctxt)
{
    DevOperStatus devOperStatus = {0};

    uint32_t sysUpTime = pdTICKS_TO_MS(xTaskGetTickCount() - appData->baseTicks) / 1000;
    blePutUINT32(devOperStatus.sysUpTime, sysUpTime);
    blePutUINT32(devOperStatus.wifiStaIpAddr, appData->wifiIpAddr);
    blePutUINT32(devOperStatus.wifiApIpAddr, appData->wifiGwAddr);
    memcpy(devOperStatus.wifiStaMacAddr, appData->wifiMac, 6);
    devOperStatus.wifiRssi = appData->wifiRssi;
    devOperStatus.wifiChan = appData->wifiPriChan;
    memcpy(devOperStatus.blePerMacAddr, inbConnInfo.ourAddr.val, 6);
    memcpy(devOperStatus.bleCenMacAddr, inbConnInfo.peerAddr.val, 6);
    blePutUINT16(devOperStatus.freeHeapMem, (heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
    blePutUINT16(devOperStatus.maxHeapMemBlk, (heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT) / 1024));
#ifdef CONFIG_FAT_FS
    uint64_t totalBytes, freeBytes;
    if (esp_vfs_fat_info(CONFIG_FAT_FS_MOUNT_POINT, &totalBytes, &freeBytes) == ESP_OK) {
        uint16_t freeFatFsSpace = freeBytes / 1024;
        blePutUINT16(devOperStatus.freeFatFsSpace, freeFatFsSpace);
    }
#endif

    return (os_mbuf_append(ctxt->om, &devOperStatus, sizeof (devOperStatus)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}
#else
static int getDevOperStatus(struct ble_gatt_access_ctxt *ctxt)
{
    WiFiConfigInfo *wifiConfigInfo = &appConfigInfo.wifiConfigInfo;
    uint32_t sysUpTime = pdTICKS_TO_MS(xTaskGetTickCount() - baseTicks) / 1000;
    char ipAddr[INET_ADDRSTRLEN];
    static char fmtBuf[256];

    snprintf(fmtBuf, sizeof (fmtBuf),
            "upTime: %lu [s]\n"
            "ipAddr: %s\n"
            "rssi: %d [dBm]\n"
            "freeMem: %u [kB]\n"
            "maxBlk: %u [kB]\n",
            sysUpTime,
            inet_ntop(AF_INET, &appData->wifiIpAddr, ipAddr, sizeof (ipAddr)),
            appData->wifiRssi,
            (unsigned) (heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024),
            (unsigned) (heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT) / 1024));

    return (os_mbuf_append(ctxt->om, fmtBuf, strlen(fmtBuf)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}
#endif

static CmdStatus cmdStatus = { .opCode = coNoOp, .status = csIdle };

static int getCmdStatus(struct ble_gatt_access_ctxt *ctxt)
{
    return (os_mbuf_append(ctxt->om, &cmdStatus, sizeof (cmdStatus)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static CmdStatusCode restartDeviceCmd(struct os_mbuf *om)
{
    return (restartDevice() == 0) ? csSuccess : csFailed;
}

static CmdStatusCode clearConfigCmd(struct os_mbuf *om)
{
    return (clearConfig() == 0) ? csSuccess : csFailed;
}

static CmdStatusCode startOtaUpdateCmd(struct os_mbuf *om)
{
#ifdef CONFIG_OTA_UPDATE
    return (otaUpdateStart() == 0) ? csSuccess : csFailed;
#else
    return csInvOpCode;
#endif
}

static CmdStatusCode setLogLevelCmd(struct os_mbuf *om)
{
    LogLevel logLevel;

    if ((om == NULL) || (om->om_len != 2)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    logLevel = om->om_data[1];
    if (logLevel > debug) {
        return csInvParam;
    }
    msgLogSetLevel(logLevel);

    return csSuccess;
}

static CmdStatusCode setLogDestCmd(struct os_mbuf *om)
{
    LogDest logDest;

    if ((om == NULL) || (om->om_len != 2)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    logDest = om->om_data[1];
    if (logDest > both) {
        return csInvParam;
    }
    msgLogSetDest(logDest);

    return csSuccess;
}

static int setUtcTimeCmd(struct os_mbuf *om)
{
    struct timeval utcTime = {0};
    uint32_t utcSecs;
    int8_t utcOffset;

    if ((om == NULL) || (om->om_len != 6)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    utcSecs = bleGetUINT32(&om->om_data[1]);
    utcTime.tv_sec = utcSecs;
    utcOffset = (int8_t) om->om_data[5];
    if ((utcOffset < -12) || (utcOffset > 12)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    mlog(trace, "utcTime=0x%08llX utcOffset=%d", utcTime.tv_sec, utcOffset);

    if (settimeofday(&utcTime, NULL) != 0) {
        return csFailed;
    }
    appData->persData.utcOffset = utcOffset;
    nvramWrite(&appData->persData);

    return csSuccess;
}

static int setUtcOffsetCmd(struct os_mbuf *om)
{
    int8_t utcOffset;

    if ((om == NULL) || (om->om_len != 2)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    utcOffset = (int8_t) om->om_data[1];
    if ((utcOffset < -12) || (utcOffset > 12)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    appData->persData.utcOffset = utcOffset;
    nvramWrite(&appData->persData);

    mlog(trace, "utcOffset=%d", utcOffset);

    return 0;
}

static int setWiFiStateCmd(struct os_mbuf *om)
{
#ifdef CONFIG_WIFI_STATION
    bool enabled = false;

    if ((om == NULL) || (om->om_len != 2)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    enabled = !!om->om_data[1];

    return (wifiEnable(appData, enabled) == 0) ? csSuccess : csFailed;
#else
    return csInvOpCode;
#endif
}

static CmdStatusCode dumpMlogFileCmd(struct os_mbuf *om)
{
    return (dumpMlogFile(true) == 0) ? csSuccess : csFailed;
}

static CmdStatusCode deleteMlogFileCmd(struct os_mbuf *om)
{
    return (deleteMlogFile(true) == 0) ? csSuccess : csFailed;
}

static int runCmd(struct ble_gatt_access_ctxt *ctxt)
{
    struct os_mbuf *om = ctxt->om;
    CmdStatusCode csc;

    if ((om == NULL) || (om->om_len < 1)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    if (cmdStatus.status == csInProg) {
        // Last command still in progress
        return 0;
    }

    cmdStatus.opCode = om->om_data[0];
    cmdStatus.status = csInProg;

    switch (cmdStatus.opCode) {
    case coNoOp:
        csc = csSuccess;
        break;

    case coRestartDevice:
        csc = restartDeviceCmd(om);
        break;

    case coClearConfig:
        csc = clearConfigCmd(om);
        break;

    case coStartOtaUpdate:
        csc = startOtaUpdateCmd(om);
        break;

    case coSetLogLevel:
        csc = setLogLevelCmd(om);
        break;

    case coSetLogDest:
        csc = setLogDestCmd(om);
        break;

    case coSetUtcTime:
        csc = setUtcTimeCmd(om);
        break;

    case coSetUtcOffset:
        csc = setUtcOffsetCmd(om);
        break;

    case coSetWiFiState:
        csc = setWiFiStateCmd(om);
        break;

    case coDumpMlogFile:
        csc = dumpMlogFileCmd(om);
        break;

    case coDeleteMlogFile:
        csc = deleteMlogFileCmd(om);
        break;

    default:
        csc = csInvOpCode;
        break;
    }

    cmdStatus.status = csc;

    if (inbConnInfo.cmdReqIndicate) {
        // Send the Command Status via a BLE indication
        struct os_mbuf *om = ble_hs_mbuf_from_flat(&cmdStatus, sizeof (cmdStatus));
        if (om != NULL) {
            ble_gatts_indicate_custom(inbConnInfo.connHandle, inbConnInfo.cmdReqHandle, om);
        }
    }

    return 0;
}

#ifdef CONFIG_DCS_SERVICE_HELP
static const char *cmdHelp = \
    "01: Restart\n"
    "02: Clear Config\n"
    "03: OTA Update\n"
    "04: MLOG Level {0=No 1=Inf 2=Trc 3=Dbg}\n"
    "05: MLOG Dest {0=Con 1=File 2=Both}\n"
    "06: UTC Time {secs since Epoch}\n"
    "07: UTC Offset {hrs from UTC}\n"
    "08: WiFi State {0=Dis 1=Ena}\n"
    "09: Dump MLOG.TXT\n"
    "0A: Delete MLOG.TXT\n";
#endif

static int deviceConfigCb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);

    if (uuid == GATT_DCS_WIFI_CREDENTIALS_UUID) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            return getWiFiCredentials(ctxt);
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            return setWiFiCredentials(ctxt);
        }
    } else if (uuid == GATT_DCS_OPERATING_STATUS_UUID) {
        return getDevOperStatus(ctxt);
    } else if (uuid == GATT_DCS_COMMAND_REQUEST_UUID) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            return getCmdStatus(ctxt);
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            return runCmd(ctxt);
        }
#ifdef CONFIG_DCS_SERVICE_HELP
    } else if (uuid == GATT_DCS_COMMAND_HELP_UUID) {
        return (os_mbuf_append(ctxt->om, cmdHelp, strlen(cmdHelp)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
#endif
    }

    return 0;
}

// Table of supported BLE services and their characteristics
static const struct ble_gatt_svc_def gattSvcs[] = {
    {
        // Standard Device Information Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Manufacturer Name
                .uuid = BLE_UUID16_DECLARE(GATT_DIS_MANUFACTURER_NAME_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Model Number
                .uuid = BLE_UUID16_DECLARE(GATT_DIS_MODEL_NUMBER_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Serial Number
                .uuid = BLE_UUID16_DECLARE(GATT_DIS_SERIAL_NUMBER_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Firmware Revision
                .uuid = BLE_UUID16_DECLARE(GATT_DIS_FIRMWARE_REVISION_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Hardware Revision
                .uuid = BLE_UUID16_DECLARE(GATT_DIS_HARDWARE_REVISION_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                0,  // No more characteristics in this service
            },
        }
    },

    {
        // Custom Device Configuration Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_CONFIG_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // WiFi Credentials
                .uuid = BLE_UUID16_DECLARE(GATT_DCS_WIFI_CREDENTIALS_UUID),
                .access_cb = deviceConfigCb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {
                // Operating Status
                .uuid = BLE_UUID16_DECLARE(GATT_DCS_OPERATING_STATUS_UUID),
                .access_cb = deviceConfigCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Command Request
                .uuid = BLE_UUID16_DECLARE(GATT_DCS_COMMAND_REQUEST_UUID),
                .access_cb = deviceConfigCb,
                .val_handle = &inbConnInfo.cmdReqHandle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_INDICATE,
            },
#ifdef CONFIG_DCS_SERVICE_HELP
            {
                // Command Help
                .uuid = BLE_UUID16_DECLARE(GATT_DCS_COMMAND_HELP_UUID),
                .access_cb = deviceConfigCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
#endif
            {
                0,  // No more characteristics in this service
            },
        }
    },

    {
        0,  // No more services
    },
};

// Format a BLE MAC address
const char *fmtBleMac(const uint8_t *addr)
{
    static char buf[20];
    snprintf(buf, sizeof (buf), "%02X:%02X:%02X:%02X:%02X:%02X",
            addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    return buf;
}

static int procConnectEvent(const struct ble_gap_event *event)
{
    if (event->connect.status == 0) {
        struct ble_gap_conn_desc connDesc = {0};
        ble_gap_conn_find(event->connect.conn_handle, &connDesc);

        // Set up connection state
        inbConnInfo.connTime = time(NULL);
        inbConnInfo.connHandle = event->connect.conn_handle;
        inbConnInfo.connEstablished = true;
        inbConnInfo.peerAddr = connDesc.peer_id_addr;

        mlog(info, "Inbound BLE connection established: connHandle=%u peer=%s",
                inbConnInfo.connHandle, fmtBleMac(inbConnInfo.peerAddr.val));

        // Let the user know...
        ledSet(on, yellow);
    }

    return 0;
}

static int procDisconnectEvent(const struct ble_gap_event *event)
{
    if (event->disconnect.conn.conn_handle == inbConnInfo.connHandle) {
        mlog(info, "Inbound BLE connection dropped: connHandle=%u reason=0x%03x", inbConnInfo.connHandle, event->disconnect.reason);

        // Clean up connection state
        inbConnInfo.connTime = 0;
        inbConnInfo.connHandle = 0;
        inbConnInfo.connEstablished = false;
        inbConnInfo.cmdReqIndicate = false;
        memset(&inbConnInfo.peerAddr, 0, sizeof (inbConnInfo.peerAddr));

        // Start advertising again
        nimbleAdvertise();

        // Let the user know...
        ledSet(off, black);
    }

    return 0;
}

static int procTermFailureEvent(const struct ble_gap_event *event)
{
    mlog(warning, "connHandle=%u status=0x%03x", event->term_failure.conn_handle, event->term_failure.status);

    return 0;
}

static int procNotificationTxEvent(const struct ble_gap_event *event)
{
    return 0;
}

static int procSubscribeEvent(const struct ble_gap_event *event)
{
    uint16_t attrHandle = event->subscribe.attr_handle;

    //mlog(trace, "connHandle=%u attrHandle=%u notify=%u indicate=%u", event->subscribe.conn_handle, attrHandle, event->subscribe.cur_notify, event->subscribe.cur_indicate);

    if (attrHandle == 8) {
        // When the iOS LightBlue mobile app connects, it
        // enables indications on the characteristic with
        // the attribute handle 8, which we don't care...
    } else if (attrHandle == inbConnInfo.cmdReqHandle) {
        inbConnInfo.cmdReqIndicate = event->subscribe.cur_indicate;
        mlog(info, "Command Request indications %sabled!", (inbConnInfo.cmdReqIndicate) ? "en" : "dis");
    } else {
        mlog(warning, "Unsupported attrHandle=%u : cmdReqHandle=%u", attrHandle, inbConnInfo.cmdReqHandle);
    }

    return 0;
}

static int nimbleGapEventCb(struct ble_gap_event *event, void *arg)
{
    //mlog(trace, "event=%u stack=%u", event->type, uxTaskGetStackHighWaterMark(NULL));

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        return procConnectEvent(event);

    case BLE_GAP_EVENT_DISCONNECT:
        return procDisconnectEvent(event);

    case BLE_GAP_EVENT_TERM_FAILURE:
        return procTermFailureEvent(event);

    case BLE_GAP_EVENT_CONN_UPDATE:
        break;

    case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
        break;

    case BLE_GAP_EVENT_DISC:
        break;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        break;

    case BLE_GAP_EVENT_NOTIFY_RX:
        break;

    case BLE_GAP_EVENT_NOTIFY_TX:
        return procNotificationTxEvent(event);

    case BLE_GAP_EVENT_SUBSCRIBE:
        return procSubscribeEvent(event);

    case BLE_GAP_EVENT_MTU:
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        break;

    case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
        break;

#if MYNEWT_VAL(BLE_ENABLE_CONN_REATTEMPT)
    case BLE_GAP_EVENT_REATTEMPT_COUNT:
        mlog(warning, "BLE_GAP_EVENT_REATTEMPT_COUNT: connHandle=%u reatemptCount=%u", event->reattempt_cnt.conn_handle, event->reattempt_cnt.count);
        break;
#endif

    case BLE_GAP_EVENT_DATA_LEN_CHG:
        break;

    case BLE_GAP_EVENT_LINK_ESTAB:
        break;

    default:
        mlog(warning, "Unhandled event! type=%u", event->type);
    }

    return 0;
}


static void nimbleAdvertise(void)
{
    struct ble_gap_adv_params advParams = {0};  // ADV_IND
    struct ble_hs_adv_fields rspFields = {0};   // SCAN_RSP
    struct ble_hs_adv_fields advFields = {0};
    const char *devName = ble_svc_gap_device_name();
    int rc;

    if ((rc = ble_hs_id_infer_auto(false, &inbConnInfo.ourAddr.type)) != 0) {
        mlog(fatal, "ble_hs_id_infer_auto: rc=%d", rc);
    }

    if ((rc = ble_hs_id_copy_addr(inbConnInfo.ourAddr.type, inbConnInfo.ourAddr.val, NULL)) != 0) {
        mlog(fatal, "ble_hs_id_copy_addr: rc=%d", rc);
    }

    /*
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info)
     *     o Advertising tx power
     *     o Device name
     *  Advertise two flags:
     *      o Discoverability in forthcoming advertisement (general)
     *      o BLE-only (BR/EDR unsupported)
     */
    advFields.flags = BLE_HS_ADV_F_DISC_GEN |
                      BLE_HS_ADV_F_BREDR_UNSUP;

    /*
     * Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    advFields.tx_pwr_lvl_is_present = true;
    advFields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    advFields.name = (uint8_t *) devName;
    advFields.name_len = strlen(devName);
    advFields.name_is_complete = true;

    if ((rc = ble_gap_adv_set_fields(&advFields)) != 0) {
        mlog(fatal, "ble_gap_adv_set_fields: rc=%d", rc);
    }

    rspFields.uuids16 = (ble_uuid16_t[]) {
#ifdef CONFIG_DEVICE_INFO_SERVICE
        // Device Info Service
        BLE_UUID16_INIT(GATT_DEVICE_INFO_SERVICE_UUID),
#endif
        // Device Config Service
        BLE_UUID16_INIT(GATT_DEVICE_CONFIG_SERVICE_UUID),
    };
#ifdef CONFIG_DEVICE_INFO_SERVICE
    rspFields.num_uuids16++;
#endif
    rspFields.num_uuids16++;
    rspFields.uuids16_is_complete = true;

    if ((rc = ble_gap_adv_rsp_set_fields(&rspFields)) != 0) {
        mlog(fatal, "ble_gap_adv_rsp_set_fields: rc=%d", rc);
    }

    /* Begin advertising */
    advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
    advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
    if ((rc = ble_gap_adv_start(inbConnInfo.ourAddr.type, NULL, BLE_HS_FOREVER, &advParams, nimbleGapEventCb, NULL)) != 0) {
        if (rc != BLE_HS_EALREADY) {
            mlog(fatal, "ble_gap_adv_start: rc=0x%03x", rc);
        }
    }

    mlog(info, "Advertising BLE services as \"%s\" ...", devName);
}

static void nimbleOnReset(int reason)
{
    mlog(error, "reason=%d", reason);
}

static void nimbleOnSync(void)
{
    //mlog(trace, " ");

    // Ensure we have a public address
    if (ble_hs_util_ensure_addr(false) != 0) {
        mlog(fatal, "No BLE public address!");
    }

#if 0
    // Show the attribute handle of the characteristics
    // which support NOTIFY/INDICATE.
    {
        for (int svc = 0; gattSvcs[svc].uuid != NULL; svc++) {
            for (int chr = 0; gattSvcs[svc].characteristics[chr].uuid != NULL; chr++) {
                if (gattSvcs[svc].characteristics[chr].flags & (BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE)) {
                    if (gattSvcs[svc].characteristics[chr].val_handle != NULL) {
                        mlog(trace, "svc=%d chr=%d attrHandle=%u", svc, chr, *gattSvcs[svc].characteristics[chr].val_handle);
                    }
                }
            }
        }
    }
#endif

    // Start advertising our own services
    nimbleAdvertise();
}

static void nimbleHostTask(void *param)
{
    // Stash the appData pointer
    appData = param;

    //mlog(trace, "Task %s started: core=%u prio=%u", __func__, esp_cpu_get_core_id(), uxTaskPriorityGet(NULL));

    // This function will return only when nimble_port_stop()
    // is executed...
    nimble_port_run();

    mlog(fatal, "SPONG! nimble_port_run() returned!");
}

int bleInit(AppData *appData)
{
    int rc;
    char devName[32];

    // Initialize controller and NimBLE host stack
    if ((rc = nimble_port_init()) != ESP_OK) {
        mlog(fatal, "nimble_port_init: rc=%d", rc);
    }

    // Initialize NimBLE host configuration
    ble_hs_cfg.sync_cb = nimbleOnSync;
    ble_hs_cfg.reset_cb = nimbleOnReset;

    // Initialize NimBLE peripheral/server configuration
    ble_svc_gap_init();
    ble_svc_gatt_init();
    snprintf(devName, sizeof (devName), "%s-%02X%02X", CONFIG_BLE_ADV_NAME, appData->serialNumber[2], appData->serialNumber[3]);
    ble_svc_gap_device_name_set(devName);
    ble_svc_gap_device_appearance_set(BLE_SVC_GAP_APPEARANCE_GEN_UNKNOWN);

    if ((rc = ble_gatts_count_cfg(gattSvcs)) != 0) {
        mlog(fatal, "ble_gatts_count_cfg: rc=%d", rc);
    }

    if ((rc = ble_gatts_add_svcs(gattSvcs)) != 0) {
        mlog(fatal, "ble_gatts_add_svcs: rc=%d", rc);
    }

    // Start the NimBLE Host task
    if ((rc = xTaskCreatePinnedToCore(nimbleHostTask, "bleHostTask", CONFIG_BLE_HOST_TASK_STACK,
                                      (void *) appData, CONFIG_BLE_HOST_TASK_PRIO, NULL, CONFIG_BLE_HOST_TASK_CPU)) != pdPASS) {
        mlog(fatal, "xTaskCreatePinnedToCore: rc=%d", rc);
    }

    return 0;
}

#endif  // CONFIG_BLE_PERIPHERAL
