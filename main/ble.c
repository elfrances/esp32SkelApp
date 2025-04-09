#include "ble.h"
#include "esp32.h"
#include "mlog.h"

#ifdef CONFIG_BLE_PERIPHERAL

// Device Info Service
#define GATT_DEVICE_INFO_UUID               0x180A
#define GATT_MANUFACTURER_NAME_UUID             0x2A29  // READ
#define GATT_MODEL_NUMBER_UUID                  0x2A24  // READ
#define GATT_SERIAL_NUMBER_UUID                 0x2A25  // READ
#define GATT_FIRMWARE_REVISION_UUID             0x2a26  // READ
#define GATT_HARDWARE_REVISION_UUID             0x2a27  // READ


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
        //snprintf(strBuf, sizeof (strBuf), "%u", server->serialNumber);
        //string = strBuf;
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

static const struct ble_gatt_svc_def gattSvcs[] = {
    {
        // Device Information Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Characteristic: Manufacturer Name
                .uuid = BLE_UUID16_DECLARE(GATT_MANUFACTURER_NAME_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Characteristic: Model Number
                .uuid = BLE_UUID16_DECLARE(GATT_MODEL_NUMBER_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Characteristic: Serial Number
                .uuid = BLE_UUID16_DECLARE(GATT_SERIAL_NUMBER_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Characteristic: Firmware Revision
                .uuid = BLE_UUID16_DECLARE(GATT_FIRMWARE_REVISION_UUID),
                .access_cb = deviceInfoCb,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                // Characteristic: Hardware Revision
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

        mlog(info, "Inbound BLE connection established: connHandle=%u peer=%s",
                event->connect.conn_handle, fmtBleMac(connDesc.peer_id_addr.val));
    }

    return 0;
}

static int procDisconnectEvent(const struct ble_gap_event *event)
{
    uint16_t connHandle = event->disconnect.conn.conn_handle;

    mlog(trace, "connHandle=%u reason=0x%03x", connHandle, event->disconnect.reason);

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
    mlog(info, "BLE_GAP_EVENT_SUBSCRIBE: connHandle=%u valHandle=%u notify=%u indicate=%u", event->subscribe.conn_handle, event->subscribe.attr_handle, event->subscribe.cur_notify, event->subscribe.cur_indicate);

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
    ble_svc_gap_device_name_set(CONFIG_BLE_ADV_NAME);
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
