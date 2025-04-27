#include "app.h"
#include "ble.h"
#include "esp32.h"
#include "mlog.h"
#include "nvram.h"
#include "ota.h"

#ifdef CONFIG_BLE_PERIPHERAL

// Forward declarations
static void nimbleAdvertise(void);

// Device Info Service
#define GATT_DEVICE_INFO_UUID               0x180A
#define GATT_MANUFACTURER_NAME_UUID             0x2A29  // READ
#define GATT_MODEL_NUMBER_UUID                  0x2A24  // READ
#define GATT_SERIAL_NUMBER_UUID                 0x2A25  // READ
#define GATT_FIRMWARE_REVISION_UUID             0x2a26  // READ
#define GATT_HARDWARE_REVISION_UUID             0x2a27  // READ

static SerialNumber serialNumber;

static int deviceInfoCb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);
    char strBuf[32];
    const char *string = "???";

    if (uuid == GATT_MANUFACTURER_NAME_UUID) {
        string = CONFIG_MANUFACTURER_NAME;
    } else if (uuid == GATT_MODEL_NUMBER_UUID) {
        string = CONFIG_MODEL_NUMBER;
    } else if (uuid == GATT_SERIAL_NUMBER_UUID) {
        snprintf(strBuf, sizeof (strBuf), "%02X%02X%02X%02X",
                serialNumber.digits[0], serialNumber.digits[1], serialNumber.digits[2], serialNumber.digits[3]);
        string = strBuf;
    } else if (uuid == GATT_FIRMWARE_REVISION_UUID) {
        const esp_app_desc_t *appDesc = esp_app_get_description();
        snprintf(strBuf, sizeof (strBuf), "%s", appDesc->version);
        string = strBuf;
    } else if (uuid == GATT_HARDWARE_REVISION_UUID) {
        string = CONFIG_IDF_TARGET;
    }

    //mlog(trace, "uuid=0x%04x value=%s", uuid, string);

    return (os_mbuf_append(ctxt->om, string, strlen(string)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// Device Config Service
#define GATT_DEVICE_CONFIG_UUID             0xFE00
#define GATT_WIFI_CREDENTIALS_UUID              0xFE01  // READ, WRITE
#define GATT_WIFI_IP_ADDRESS_UUID               0xFE02  // READ
#define GATT_UTC_OFFSET_UUID                    0xFE03  // READ, WRITE
#define GATT_COMMAND_UUID                       0xFE04  // READ, WRITE

static int getWiFiCredentials(struct ble_gatt_access_ctxt *ctxt)
{
    const WiFiConfigInfo *wifiConfigInfo = &appConfigInfo.wifiConfigInfo;
    uint8_t wifiCred[130];
    size_t ssidLen = strlen(wifiConfigInfo->wifiSsid) + 1;
    size_t passwdLen = strlen(wifiConfigInfo->wifiPasswd) + 1;
    memcpy(wifiCred, wifiConfigInfo->wifiSsid, ssidLen);
    memcpy((wifiCred + ssidLen), wifiConfigInfo->wifiPasswd, passwdLen);
    return (os_mbuf_append(ctxt->om, wifiCred, (ssidLen + passwdLen)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int setWiFiCredentials(struct ble_gatt_access_ctxt *ctxt)
{
    WiFiConfigInfo *wifiConfigInfo = &appConfigInfo.wifiConfigInfo;
    struct os_mbuf *om = ctxt->om;
    const char *s;

    // The format of the WiFi credentials data is the
    // SSID string followed by the Password string, in
    // both cases including the terminating null char.
    // For example, if SSID="ABC" and Password="123" the
    // data written would be the 8-byte hex sequence:
    // 41, 42, 43, 00, 31, 32, 33, 00.
    if ((om == NULL) || (om->om_data == NULL) || (om->om_len > (sizeof (wifiConfigInfo->wifiSsid) + sizeof (wifiConfigInfo->wifiPasswd)))) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    s = (char *) om->om_data;
    if (strlen(s) >= sizeof (wifiConfigInfo->wifiSsid)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }
    strcpy(wifiConfigInfo->wifiSsid, s);

    s += strlen(s) + 1;
    if (strlen(s) >= sizeof (wifiConfigInfo->wifiPasswd)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }
    strcpy(wifiConfigInfo->wifiPasswd, s);

    nvramWrite(&appConfigInfo);
    mlog(trace, "wifiSsid=%s wifiPasswd=%s", wifiConfigInfo->wifiSsid, wifiConfigInfo->wifiPasswd);

    return 0;
}

static int getWiFiIpAddress(struct ble_gatt_access_ctxt *ctxt)
{
    WiFiConfigInfo *wifiConfigInfo = &appConfigInfo.wifiConfigInfo;
    return (os_mbuf_append(ctxt->om, &wifiConfigInfo->wifiIpAddr, sizeof (wifiConfigInfo->wifiIpAddr)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int getUtcOffset(struct ble_gatt_access_ctxt *ctxt)
{
    return (os_mbuf_append(ctxt->om, &appConfigInfo.utcOffset, sizeof (appConfigInfo.utcOffset)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int setUtcOffset(struct ble_gatt_access_ctxt *ctxt)
{
    struct os_mbuf *om = ctxt->om;
    int8_t utcOffset;

    if ((om == NULL) || (om->om_len != 1)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    utcOffset = (int8_t) om->om_data[0];
    if ((utcOffset < -12) || (utcOffset > 12)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    appConfigInfo.utcOffset = utcOffset;
    nvramWrite(&appConfigInfo);
    mlog(trace, "utcOffset=%d", utcOffset);

    return 0;
}

typedef enum CmdOpCode {
    coNoOp = 0,
    coRestartDevice,
    coClearConfig,
    coStartOtaUpdate,
    coSetLogLevel,
    coMax
} CmdOpCode;

typedef enum CmdStatus {
    csIdle = 0,
    csInProg,
    csSuccess,
    csFailed,
    csInvOpCode,
    csInvParam,
} CmdStatus;

static CmdStatus cmdStatus = csIdle;

static int getCmdStatus(struct ble_gatt_access_ctxt *ctxt)
{
    uint8_t cs = cmdStatus;
    return (os_mbuf_append(ctxt->om, &cs, sizeof (cs)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static CmdStatus restartDeviceCmd(struct os_mbuf *om)
{
    mlog(info, "Restarting the device...");
    esp_restart();
    return csSuccess;
}

static CmdStatus clearConfigCmd(struct os_mbuf *om)
{
    mlog(info, "Clearing the device configuration...");
    return (nvramClear() == 0) ? csSuccess : csFailed;
}

static CmdStatus startOtaUpdateCmd(struct os_mbuf *om)
{
#ifdef CONFIG_OTA_UPDATE
    return (otaUpdateStart() == 0) ? csSuccess : csFailed;
#else
    return csInvOpCode;
#endif
}

static CmdStatus setLogLevelCmd(struct os_mbuf *om)
{
    LogLevel logLevel = om->om_data[1];
    if (logLevel > debug) {
        return csInvParam;
    }
    msgLogSetLevel(logLevel);
    return csSuccess;
}

static int runCmd(struct ble_gatt_access_ctxt *ctxt)
{
    struct os_mbuf *om = ctxt->om;
    CmdOpCode opCode;

    if ((om == NULL) || (om->om_len < 1)) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }

    if (cmdStatus == csInProg) {
        // Last command still in progress
        return 0;
    }

    opCode = om->om_data[0];
    cmdStatus = csInProg;

    switch (opCode) {
    case coNoOp:
        cmdStatus = csSuccess;
        break;

    case coRestartDevice:
        cmdStatus = restartDeviceCmd(om);
        break;

    case coClearConfig:
        cmdStatus = clearConfigCmd(om);
        break;

    case coStartOtaUpdate:
        cmdStatus = startOtaUpdateCmd(om);
        break;

    case coSetLogLevel:
        cmdStatus = setLogLevelCmd(om);
        break;

    default:
        cmdStatus = csInvOpCode;
        break;
    }

    return 0;
}

static int deviceConfigCb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);

    if (uuid == GATT_WIFI_CREDENTIALS_UUID) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            return getWiFiCredentials(ctxt);
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            return setWiFiCredentials(ctxt);
        }
    } else if (uuid == GATT_WIFI_IP_ADDRESS_UUID) {
        return getWiFiIpAddress(ctxt);
    } else if (uuid == GATT_UTC_OFFSET_UUID) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            return getUtcOffset(ctxt);
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            return setUtcOffset(ctxt);
        }
    } else if (uuid == GATT_COMMAND_UUID) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            return getCmdStatus(ctxt);
        } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            return runCmd(ctxt);
        }
    }

    return 0;
}

// Table of supported BLE services and their characteristics
static const struct ble_gatt_svc_def gattSvcs[] = {
    {
        // Standard Device Information Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Manufacturer Name
                .uuid = BLE_UUID16_DECLARE(GATT_MANUFACTURER_NAME_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Model Number
                .uuid = BLE_UUID16_DECLARE(GATT_MODEL_NUMBER_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Serial Number
                .uuid = BLE_UUID16_DECLARE(GATT_SERIAL_NUMBER_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Firmware Revision
                .uuid = BLE_UUID16_DECLARE(GATT_FIRMWARE_REVISION_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Hardware Revision
                .uuid = BLE_UUID16_DECLARE(GATT_HARDWARE_REVISION_UUID),
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
        .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_CONFIG_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // WiFi Credentials
                .uuid = BLE_UUID16_DECLARE(GATT_WIFI_CREDENTIALS_UUID),
                .access_cb = deviceConfigCb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE
            },
            {
                // WiFi IP Address
                .uuid = BLE_UUID16_DECLARE(GATT_WIFI_IP_ADDRESS_UUID),
                .access_cb = deviceConfigCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // UTC Offset
                .uuid = BLE_UUID16_DECLARE(GATT_UTC_OFFSET_UUID),
                .access_cb = deviceConfigCb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE
            },
            {
                // Command
                .uuid = BLE_UUID16_DECLARE(GATT_COMMAND_UUID),
                .access_cb = deviceConfigCb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE
            },
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

// Inbound Connection Information
typedef struct InbConnInfo {
    time_t connTime;
    uint16_t connHandle;
    bool connEstablished;
    ble_addr_t peerAddr;
} InbConnInfo;

static InbConnInfo inbConnInfo;

static int procConnectEvent(const struct ble_gap_event *event)
{
    if (event->connect.status == 0) {
        struct ble_gap_conn_desc connDesc = {0};
        ble_gap_conn_find(event->connect.conn_handle, &connDesc);

        inbConnInfo.connTime = time(NULL);
        inbConnInfo.connHandle = event->connect.conn_handle;
        inbConnInfo.connEstablished = true;
        inbConnInfo.peerAddr = connDesc.peer_id_addr;

        mlog(info, "Inbound BLE connection established: connHandle=%u peer=%s",
                inbConnInfo.connHandle, fmtBleMac(inbConnInfo.peerAddr.val));
    }

    return 0;
}

static int procDisconnectEvent(const struct ble_gap_event *event)
{
    if (event->disconnect.conn.conn_handle == inbConnInfo.connHandle) {
        mlog(info, "Inbound BLE connection dropped: connHandle=%u reason=0x%03x", inbConnInfo.connHandle, event->disconnect.reason);
        memset(&inbConnInfo, 0, sizeof (inbConnInfo));
        nimbleAdvertise();
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
    // When the iOS LightBlue mobile app connects, it
    // enables indications on the characteristic with
    // the attribute handle 8, which we don't care...
    if (event->subscribe.attr_handle != 8) {
        mlog(info, "BLE_GAP_EVENT_SUBSCRIBE: connHandle=%u valHandle=%u notify=%u indicate=%u", event->subscribe.conn_handle, event->subscribe.attr_handle, event->subscribe.cur_notify, event->subscribe.cur_indicate);
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


static uint8_t advAddrType;

static void nimbleAdvertise(void)
{
    ble_addr_t advAddr = {0};
    struct ble_gap_adv_params advParams = {0};  // ADV_IND
    struct ble_hs_adv_fields rspFields = {0};   // SCAN_RSP
    struct ble_hs_adv_fields advFields = {0};
    const char *devName = ble_svc_gap_device_name();
    int rc;

    if ((rc = ble_hs_id_infer_auto(false, &advAddrType)) != 0) {
        mlog(fatal, "ble_hs_id_infer_auto: rc=%d", rc);
    }

    if ((rc = ble_hs_id_copy_addr(advAddrType, advAddr.val, NULL)) != 0) {
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

#ifdef DEVICE_INFO_SERVICE
    // Device Info Service
    rspFields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(GATT_DEVICE_INFO_UUID)
    };
    rspFields.num_uuids16 = 1;
    rspFields.uuids16_is_complete = true;
#endif

    if ((rc = ble_gap_adv_rsp_set_fields(&rspFields)) != 0) {
        mlog(fatal, "ble_gap_adv_rsp_set_fields: rc=%d", rc);
    }

    /* Begin advertising */
    advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
    advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
    if ((rc = ble_gap_adv_start(advAddrType, NULL, BLE_HS_FOREVER, &advParams, nimbleGapEventCb, NULL)) != 0) {
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
    mlog(trace, " ");

    // Ensure we have a public address
    if (ble_hs_util_ensure_addr(false) != 0) {
        mlog(fatal, "No BLE public address!");
    }

    // Start advertising our own services
    nimbleAdvertise();
}

static void nimbleHostTask(void *param)
{
    mlog(info, "Task %s started: core=%u prio=%u", __func__, esp_cpu_get_core_id(), uxTaskPriorityGet(NULL));

    // This function will return only when nimble_port_stop()
    // is executed...
    nimble_port_run();

    mlog(fatal, "SPONG! nimble_port_run() returned!");
}

int bleInit(void)
{
    int rc;
    char devName[32];

    // Initialize controller and NimBLE host stack
    if ((rc = nimble_port_init()) != ESP_OK) {
        mlog(fatal, "nimble_port_init: rc=%d", rc);
    }

    // Get our serial number
    getSerialNumber(&serialNumber);

    // Initialize NimBLE host configuration
    ble_hs_cfg.sync_cb = nimbleOnSync;
    ble_hs_cfg.reset_cb = nimbleOnReset;

    // Initialize NimBLE peripheral/server configuration
    ble_svc_gap_init();
    ble_svc_gatt_init();
    snprintf(devName, sizeof (devName), "%s-%02X%02X", CONFIG_BLE_ADV_NAME, serialNumber.digits[2], serialNumber.digits[3]);
    ble_svc_gap_device_name_set(devName);
    ble_svc_gap_device_appearance_set(BLE_SVC_GAP_APPEARANCE_GEN_UNKNOWN);

    if ((rc = ble_gatts_count_cfg(gattSvcs)) != 0) {
        mlog(fatal, "ble_gatts_count_cfg: rc=%d", rc);
    }

    if ((rc = ble_gatts_add_svcs(gattSvcs)) != 0) {
        mlog(fatal, "ble_gatts_add_svcs: rc=%d", rc);
    }

    // Start the NimBLE Host task
    if ((rc = xTaskCreatePinnedToCore(nimbleHostTask, "bleHostTask", CONFIG_NIMBLE_TASK_STACK_SIZE,
                                      NULL, CONFIG_BLE_HOST_TASK_PRIO, NULL, tskNO_AFFINITY)) != pdPASS) {
        mlog(fatal, "xTaskCreatePinnedToCore: rc=%d", rc);
    }

    return 0;
}

#endif  // CONFIG_BLE_PERIPHERAL
