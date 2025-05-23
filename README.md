# Introduction

**esp32SkelApp** (or **SkelApp** for short) is an ESP-IDF skeletal app for the ESP32 SoC that can be used as a template when creating a new app that uses WiFi and/or BLE.
The optional features are configured using the "SKELAPP Configuration" section in the sdkconfig, and include:

### RGB LED

Provides an API for controling the color and mode of the RGB LED, to indicate relevant system events or states.  The supported colors are: black, blue, cyan, green, magenta, orange, purple, red, white, yellow.  The supporting modes are: solid on or blinking at 1, 2, or 4 times per second.

The table below describes a few uses of the RGB LED by the **SkelApp**:

| State | Color | Mode |
| ------ | ------- | ---------- | 
| Connecting to WiFi | Blue | 4 bps |
| WiFi Connection Established | Blue | solid |
| Invalid or Missing WiFi Credentials | Magenta | 4 bps |
| Inbound BLE Connection Established | Yelow | solid |
| OTA Update In Progress | Cyan | 4 bps |
| OTA Update Failure | Red | 4 bps |
| Fatal Error | Red | solid |

> [!NOTE]
> The RGB LED is controlled by a dedicated GPIO pin on the ESP32. The GPIO pin number depends on the specific ESP32 board being used, so make sure you select the right value.
>
> For Espressif DevKit boards, the default GPIO pin value in the auto-generated sdkconfig file is selected based on the type of ESP32 SoC being used: C3 or S3.

### Message Logging

Provides an API for logging messages to the console or to a file on the flash FAT FS.

### BLE Peripheral

Adds support for BLE peripheral functionality, so that an external BLE central can discover and connect to the ESP32 device to configure it.

### WiFi Station

Adds support for WiFi station, so that the ESP32 device can connect to a WiFi network to get Internet connectivity. The credentials to join the WiFi network can be provisioned manually or obtained directly from the WiFi router via WPS.

### Web Server

Adds support for a basic HTTP web server, that can be used to serve documents to a remote client.

### OTA Update

Adds support for doing OTA firmware updates over WiFi.

### FAT File System

Creates a FAT file system using the 'storage' partition in the flash memory.


# How to use SkelApp

> [!NOTE]
> This document assumes the user is familiar with the typical workflow of building an ESP-IDF project, either using the command line tools from a terminal, or using the ESP-IDF plug-in with the VScode or Eclipse IDE's.
>
> There are several sample sdkconfig files available, for different ESP32 target devices and with support for different features. See the table below:
>
> | File | Target Device | LED | MLOG | BLE | WiFi | OTA | FAT |
> | --- | --- | --- | --- | --- | --- | --- | --- |
> | sdkconfig.esp32c3.basic | ESP32-C3 DevKit-C02 | x | x | x | x |   |   |
> | sdkconfig.esp32c3.ota+fat | ESP32-C3 DevKit-C02 | x | x | x | x | x | x |
> | sdkconfig.esp32S3.basic | ESP32-S3 DevKit-C1 | x | x | x | x |   |   |
> | sdkconfig.esp32S3.ota+fat | ESP32-S3 DevKit-C1 | x | x | x | x | x | x |

> [!TIP]
> It is adviced that you first build the image using one of the provided sdkconfig files. Once you get it to build and run OK on your device, then you can customize the sdkconfig to suit the needs of your app.

For the purpose of this example, let's assume that:

* The app your are developing is called "myNewApp".
* The target device is an ESP32-C3 DevKit board.
* There is no need for OTA or FAT.

1. Download the code from GitHub and unzip it:

```bash
wget https://github.com/elfrances/esp32SkelApp/archive/refs/heads/main.zip
unzip main.zip
```

2. Rename the top level directory from "esp32SkelApp-main" to "myNewApp":

```bash
mv esp32SkelApp-main myNewApp
cd myNewApp
```

3. Fix the project name in the top-level CMakeLists.txt file:

```bash
sed -i s'/esp32SkelApp/myNewApp/' CMakeLists.txt
```

4. Use the sample sdkconfig.esp32c3.basic file to create the project's sdkconfig file:

```bash
cp sdkconfig.esp32c3.basic sdkconfig
```

5. Build and flash the app:

```bash
idf.py build
idf.py flash -p /dev/ttyUSB0
```

6. Run the ESP-IDF menuconfig command and select the desired options in the "Skeletal App Configuration" section, which comes right after the "Partition Table" setting:

> [!IMPORTANT]
> Some of the settings in the "Skeletal App Configuration" section depend on ESP-IDF "Component config" settings. For example, you won't be able to enable the SkelApp's BLE Peripheral setting unless the Bluetooth feature is enabled.
>
> If your app will not use OTA to update the firmware, then you can regain significant flash memory space by eliminating the OTA partitions in the myNewApp/partitions.csv file.
>
> Similarly, if you don't need FAT FS support, you can eliminate the "storage" partition, and give the space back to the factory/ota partitions.

```bash
idf.py menuconfig
```
 
7. Add your own app's code to the appMainTask() in myNewApp/main/app.c.  This task runs a simple infinite work loop, with the period specified by the config attribute MAIN_TASK_WAKEUP_PERIOD. Of course you are free to replace this simple periodic work loop, for something more advanced, such as an event-driven work loop that can process different events posted by timers, interrupts, callback's, etc.

The data used by the app is stored in the AppData structure defined in myNewApp/main/app.h:

```c
// App's data
typedef struct AppData {
    AppPersData persData;   // saved to (restored from) NVM
    const char *buildType;
    const esp_app_desc_t *appDesc;
    struct timeval baseTime;
    TickType_t baseTicks;
    uint8_t serialNumber[4];

    uint8_t wifiMac[6];     // WiFi MAC address
    int8_t wifiRssi;        // WiFi RSSI value [in dBm]
    uint8_t wifiPriChan;    // WiFi primary channel
    uint32_t wifiIpAddr;    // DHCP assigned IP address
    uint32_t wifiGwAddr;    // WiFi router IP address

#ifdef CONFIG_APP_MAIN_TASK
    TaskHandle_t appMainTaskHandle;
#if CONFIG_APP_MAIN_TASK_WAKEUP_METHOD_ESP_TIMER
    esp_timer_handle_t wakeupTimerHandle;
#endif
#ifdef CONFIG_MAIN_TASK_TIME_WORK_LOOP
    uint64_t workLoopCount;
    uint64_t sumWorkLoopTime;
    uint64_t minWorkLoopTime;
    uint64_t maxWorkLoopTime;
    uint64_t avgWorkLoopTime;
#endif
#endif

    // Add your custom app data below

} AppData;
```

```c
// Custom app initialization
static int appCustInit(AppData *appData)
{
    // TBD
    return 0;
}

// This is the app's main task. It runs an infinite work
// loop, trying its best to keep a constant wake up interval.
#if CONFIG_APP_MAIN_TASK_WAKEUP_METHOD_TASK_DELAY
void appMainTask(void *parms)
{
    AppData *appData = parms;
    const TickType_t wakeupPeriodTicks = pdMS_TO_TICKS(CONFIG_MAIN_TASK_WAKEUP_PERIOD);

    appData->appMainTaskHandle = xTaskGetCurrentTaskHandle();

    // Do any custom app initialization before
    // entering the infinite work loop.
    if (appCustInit(appData) != 0) {
        mlog(fatal, "Custom app initialization failed!");
    }

    while (true) {
        TickType_t startTicks, elapsedTicks;

        // Record the start of this new pass of our
        // work loop.
        startTicks = xTaskGetTickCount();

        // Custom app code goes here...
        {
            mlog(info, "TICK!");
        }

        // Figure out how much time we spent so far
        elapsedTicks = xTaskGetTickCount() - startTicks;

        if (elapsedTicks < wakeupPeriodTicks) {
            // Sleep until the next poll period...
            TickType_t delayTicks = wakeupPeriodTicks - elapsedTicks;
            vTaskDelay(delayTicks);
        } else if (elapsedTicks > wakeupPeriodTicks) {
            // Oops! We exceeded the required wake up period!
            mlog(warning, "%u ms wakeup period exceeded by %lu ms !!!", CONFIG_MAIN_TASK_WAKEUP_PERIOD, pdTICKS_TO_MS(elapsedTicks - wakeupPeriodTicks));
        }
    }

    vTaskDelete(NULL);
}
```

> [!NOTE]
> If your app needs to store any data in non-volatile memory (NVM), you can extend **SkelApp**'s AppPersData structure, defined in myNewApp/main/app.h.  AppPersData is stored in the NVM partition of the flash memory, and is automatically loaded into memory when the app starts up.

```c
// App's persistent data
typedef struct AppPersData {
    char wifiSsid[64];      // SSID string
    char wifiPasswd[64];    // Password string
    int8_t utcOffset;       // UTC offset (in hours)

    // Add your custom app persistent data below

} AppPersData;
```

# BLE Peripheral Feature

**SkelApp** operates as a BLE peripheral device that advertises itself under the name "BLE_ADV_NAME-NNNN", where BLE_ADV_NAME is the string value set in the sdkconfig, and NNNN are four hex digits derived from the serial number of the device.

It advertises the following BLE services:

1. The standard Bluetooth SIG "Device Information Service" (DIS), which is used to report the app's serial number, firmware version, etc.

2. A custom "Device Configuration Service" (DCS), which can be used to manually configure the WiFi credentials, set the UTC offset, perform a firmware upgrade, restart the device, etc.  This service uses a configurable 16-bit UUID, and can be accessed using a generic BLE explorer app, such as [LightBlue](https://punchthrough.com/lightblue/), or by using a custom-designed iOS/Android app.

The characteristics in the DCS get their UUID's from the UUID of the service, such that characteristic number N gets assigned the value: DCS UUID plus N.

The DCS supports the following characteristics (assuming the default DCS UUID 0xFE00):

### FE01: WiFi Credentials

Properties: READ WRITE

This characteristic is used to manually set the WiFi credentials. The value is a UTF-8 string that includes the SSID and Password strings concatenated together, using the character sequence "###" as a separator.

For example, if the SSID is "HomeSweetHome" and the Password is "TopSecret!", the UTF-8 string to be written would be: "HomeSweetHome###TopSecret!".

### FE02: Operating Status

Properties: READ

This read-only characteristic is used to obtain the operating status of the ESP32 device. The data has the following format:

| Offset | Description | Data |
| ------ | ----------- | ---- |
| 0x00   | System Up Time | {UINT32: # seconds since boot} |
| 0x04   | WiFi Station IP Address | {UINT32: IPv4 address} |
| 0x08   | WiFi Access Point IP Address | {UINT32: IPv4 address} |
| 0x0C   | WiFi Station MAC Address | {UINT8[6]: IEEE 802.3 MAC Address} |
| 0x12   | WiFi RSSI | {INT8: RSSI in dBm} |
| 0x13   | WiFi Channel | {UINT8: channel number} |
| 0x14   | BLE Peripheral MAC Address | {UINT8[6]: IEEE 802.3 MAC Address} |
| 0x1A   | BLE Central MAC Address | {UINT8[6]: IEEE 802.3 MAC Address} |
| 0x20   | Free Memory | {UINT16: Free Memory in KB} |
| 0x22   | Max Free Block | {UINT16: Max Free Memory Block in KB} |
| 0x24   | TBD | |

All values are stored using Bluetooth's native little-endian encoding.

### FE03: Command Request

Properties: READ WRITE INDICATE

This characteristic is used to direct the ESP32 device to execute a command.  The supported commands are:

| OpCode | Command | Parameters |
| ------ | ------- | ---------- |
| 0x00   | No Operation   | none       |
| 0x01   | Restart Device | none |
| 0x02   | Clear Config | none |
| 0x03   | Start OTA Firmware Update | none|
| 0x04   | Set MLOG Level | {UINT8: 0=NONE, 1=INFO, 2=TRACE, 3=DEBUG} |
| 0x05   | Set MLOG Destination | {UINT8: 0=Console, 1=File, 2=Both} |
| 0x06   | Set UTC Time | {UINT32: # seconds since the Epoch, INT8: # hours east or west from GMT } |
| 0x07   | Set UTC Offset | {INT8: # hours east or west from GMT } |
| 0x08   | Set WiFi State | {UINT8: 0=Disabled, 1=Enabled} |
| 0x09   | Dump MLOG File | none |
| 0x0A   | Delete MLOG File | none |

For example:

1. To set the UTC Time to May 2, 2025, 13:10 and the timezone to Pacific Daylight Time (UTC-7) the command request bytes would be: 06 67 F8 D9 B8 F9.
2. To set just the UTC Offset to Central Daylight Time (UTC-5) the command request bytes would be: 07 FB.

Reading this characteristic returns the command execution status, which consists of two UINT8 values: the first one indicates the OpCode of the last command request issued, and the second one indicates the status of the operation. The possible values are:

| Status Code | Description |
| ----------- | ----------- |
| 0x00 | Idle |
| 0x01 | Command In Progress |
| 0x02 | Success |
| 0x03 | Failed |
| 0x04 | Invalid OpCode |
| 0x05 | Invalid Parameter |

If indications are enabled on this characteristic, the command execution status is sent in a BLE indication right after the operation completes.

### FE04: Command Help

Properties: READ

This optional characteristic can be used to get help about the commands supported by the Command Request characteristic. When read, it returns the following UTF-8 string:

```text
01: Restart
02: Clear Config
03: OTA Update
04: MLOG Level {0=No 1=Inf 2=Trc 3=Dbg}
05: MLOG Dest {0=Con 1=File 2=Both}
06: UTC Time
07: UTC Offset
08: WiFi State {0=Dis 1=Ena}
09: Dump MLOG.TXT
0A: Delete MLOG.TXT
```

# Example

Using the following SDK Configuration: 

![Skeletal-App-Configuration](/assets/Skeletal-App-Configuration.png) 

the firmware would print the following messages on the terminal when it boots up:

![Skeletal-App-Running](/assets/Skeletal-App-Running.png) 

The following screenshots show the iOS LightBlue app connected to the ESP32-C3 device that's running the **SkelApp**:

![LightBlue-1](/assets/IMG_5381.PNG)


![LightBlue-2](/assets/IMG_5382.PNG)


![LightBlue-3](/assets/IMG_5383.PNG)


![LightBlue-4](/assets/IMG_5384.PNG)

# References

[Espressif IoT Development Framework](https://github.com/espressif/esp-idf)

[Espressif ESP32-C3 DevKitC-02 Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-devkitc-02/index.html)

[Espressif ESP32-S3 DevKitC-1 Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html)

[Waveshare ESP32-C3-Zero Board](https://www.waveshare.com/wiki/ESP32-C3-Zero)

[Waveshare ESP32-S3-Zero Board](https://www.waveshare.com/wiki/ESP32-S3-Zero)



