#include "esp32.h"
#include "mlog.h"
#include "nvram.h"

static const char *configInfoBlobName = "configInfo";
static nvs_handle_t nvsHandle;

int nvramOpen(void)
{
    esp_err_t rc;

    if ((rc = nvs_open("storage", NVS_READWRITE, &nvsHandle)) != ESP_OK) {
        mlog(error, "Failed to open \"storage\" namespace! rc=0x%04x", rc);
        return -1;
    }

    return 0;
}

int nvramClose(void)
{
    if (nvsHandle == 0) {
        mlog(error, "Namespace not open!");
        return -1;
    }

    nvs_close(nvsHandle);
    nvsHandle = 0;

    return 0;
}

int nvramRead(AppPersData *configInfo)
{
    size_t configInfoBlobSize = sizeof (AppPersData);
    size_t blobLen;
    esp_err_t rc;

    //mlog(info, "configInfoBlobSize=%zu", configInfoBlobSize);

    if (nvsHandle == 0) {
        mlog(error, "Namespace not open!");
        return -1;
    }

    memset(configInfo, 0, configInfoBlobSize);

    // Find out the current size of the "confInfo" blob
    // stored in NVRAM.
    blobLen = 0;
    rc = nvs_get_blob(nvsHandle, configInfoBlobName, NULL, &blobLen);
    if (rc == ESP_ERR_NVS_NOT_FOUND) {
        mlog(warning, "\"%s\" blob not initialized...", configInfoBlobName);
        return nvramWrite(configInfo);
    } else if (rc != ESP_OK) {
        mlog(error, "Can't get size of \"%s\" blob: rc=0x%04x", configInfoBlobName, rc);
        return -1;
    }

    if (blobLen > configInfoBlobSize) {
        // In general we only expect the size of the "confInfo"
        // blob to grow, as new features are added to the firmware.
        // So having the size of this blob in the current image be
        // smaller than the size stored in NVRAM is odd. It could
        // happen when a feature is deprecated and its config info
        // happens to be at the end of the blob, so that it can be
        // safely trimmed out to save space. In this case we just
        // read the (larger) blob from NVRAM, trim it to make its
        // size match the size used by this new firmware image, and
        // we write it back to NVRAM.
        mlog(warning, "\"%s\" blob shrunk: old=%zu new=%zu", configInfoBlobName, blobLen, configInfoBlobSize);
        uint8_t *blobBuf = malloc(blobLen);
        nvs_get_blob(nvsHandle, configInfoBlobName, blobBuf, &blobLen);
        nvs_erase_all(nvsHandle);
        memcpy(configInfo, blobBuf, configInfoBlobSize);
        nvramWrite(configInfo);
        free(blobBuf);
    } else {
        blobLen = configInfoBlobSize;
        nvs_get_blob(nvsHandle, configInfoBlobName, configInfo, &blobLen);
        if (blobLen < configInfoBlobSize) {
            // This new firmware uses a larger "confInfo" blob than
            // what's currently stored in memory. We just keep the
            // current configuration info and zero-extend it to match
            // the current size.
            mlog(warning, "\"%s\" blob grew: old=%zu new=%zu", configInfoBlobName, blobLen, configInfoBlobSize);
            nvramWrite(configInfo);
        }
    }

    //mlog(info, "NVRAM's confInfo record:");
    //hexDump(confInfo, configInfoBlobSize);

    return 0;
}

int nvramWrite(const AppPersData *configInfo)
{
    size_t configInfoBlobSize = sizeof (AppPersData);
    esp_err_t rc;

    //mlog(info, "Saving configuration on NVRAM...");
    //hexDump(confInfo, configInfoBlobSize);

    if (nvsHandle == 0) {
        mlog(error, "Namespace not open!");
        return -1;
    }

    if ((rc = nvs_set_blob(nvsHandle, configInfoBlobName, configInfo, configInfoBlobSize)) != ESP_OK) {
        mlog(error, "nvs_set_blob: rc=%x", rc);
        return -1;
    }

    if ((rc = nvs_commit(nvsHandle)) == ESP_OK) {
        mlog(trace, "Configuration saved in NVRAM");
    } else {
        mlog(error, "nvs_commit: rc=%x", rc);
        return -1;
    }

    return 0;
}

int nvramClear(void)
{
    AppPersData configInfo = {0}; // this voids the MAGIC_NUMBER
    esp_err_t rc;

    //mlog(info, "Clearing confInfo record...");

    if (nvsHandle == 0) {
        mlog(error, "Namespace not open!");
        return -1;
    }

    if ((rc = nvs_erase_all(nvsHandle)) != ESP_OK) {
        mlog(error, "nvs_erase_all: rc=%x", rc);
        return -1;
    }

    return nvramWrite(&configInfo);
}
