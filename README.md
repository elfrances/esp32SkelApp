# Introduction

**esp32SkelApp** (or **SkelApp** for short) is an ESP-IDF skeletal app for the ESP32 SoC that can be used as a template when creating a new app that uses WiFi and/or BLE.
The optional features are configured using the "Skeletal App Configuration" section in the sdkconfig, and include:

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

Adds support for WiFi station, so that the ESP32 device can connect to a WiFi network to get Internet connectivity.

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

```
wget https://github.com/elfrances/esp32SkelApp/archive/refs/heads/main.zip
unzip main.zip
```

2. Rename the top level directory from "esp32SkelApp-main" to "myNewApp":

```
mv esp32SkelApp-main myNewApp
cd myNewApp
```

3. Fix the project name in the top-level CMakeLists.txt file:

```
sed -i s'/esp32SkelApp/myNewApp/' CMakeLists.txt
```

4. Use the sample sdkconfig.esp32c3.basic file to create the project's sdkconfig file:

```
cp sdkconfig.esp32c3.basic sdkconfig
```

5. Build and flash the app:

```
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

```
idf.py menuconfig
```
 
7. Add your own app's code to the appMainTask() in myNewApp/main/app.c.  This task runs a simple infinite work loop, with the period specified by the config attribute MAIN_TASK_WAKEUP_PERIOD. Of course you are free to replace this simple periodic work loop, for something more advanced, such as an event-driven work loop that can process different events posted by timers, interrupts, callback's, etc.

> [!NOTE]
> If your app needs to store any data in non-volatile storage, you can extend **SkelApp**'s AppConfigInfo structure, defined in myNewApp/main/app.h.  AppConfigInfo is stored in the NVM partition of the flash memory, and is loaded early on during app start up.


# BLE Peripheral Feature

**SkelApp** operates as a BLE peripheral device that advertises itself under the name "SKELAPP-NNNN", where NNNN are four hex digits derived from the serial number of the device.

It advertises the following BLE services:

1. The standard Bluetooth SIG "Device Information Service", which is used to report the app's serial number, firmware version, etc.

2. A custom "Device Configuration Service" using the reserved 16-bit UUID 0xFE00.  This service can be used to manually configure the WiFi credentials, set the UTC offset, perform a firmware upgrade, restart the device, etc.  This service can be accessed using a generic BLE explorer app, such as [LightBlue](https://punchthrough.com/lightblue/) or by using a custom-designed iOS/Android app.

The Device Configuration Service supports the following characteristics:

### FE01: WiFi Credentials

This characteristic is used to manually set the WiFi credentials. The value is a UTF-8 string that includes the SSID and Password strings concatenated together, using the character sequence "###" as a separator.

For example, if the SSID is "HomeSweetHome" and the Password is "TopSecret!", the UTF-8 string to be written would be: "HomeSweetHome###TopSecret!".

### FE02: WiFi IP Address

This read-only characteristic is used to obtain the IPv4 address assigned to the ESP32-C3 device.  It consists of four UINT8 values that encode the IPv4 address in network-byte order.

For example, the address 192.168.0.16 would be encoded as: C0 A8 00 10. 

### FE03: Command Request

This characteristic is used to direct the ESP32-C3 device to execute a command.  The supported commands are:

| OpCode | Command | Parameters |
| ------ | ------- | ---------- |
| 0x00   | No Operation   | none       |
| 0x01   | Restart Device | none |
| 0x02   | Clear Config | none |
| 0x03   | OTA Firmware Update | none|
| 0x04   | Set MLOG Level | {UINT8: 0=NONE, 1=INFO, 2=TRACE, 3=DEBUG} |
| 0x05   | Set MLOG Destination | {UINT8: 0=Console, 1=File, 2=Both} |
| 0x06   | Set UTC Time | {UINT32: # seconds since the Epoch }
| 0x07   | Set UTC Offset | {INT8: # hours east or west from GMT } |
| 0x08   | WiFi | {UINT8: 0=Disabled, 1=Enabled}
| 0x09   | Dump MLOG File | none

For example:

1. To set the UTC Time to May 2, 2025, 13:10 the command request bytes would be: 06 67 F8 D9 B8.
2. To set the UTC Offset to Pacific Standard Time (UTC-8) the command request bytes would be: 07 F8.

Reading this characteristic returns the command execution status, which consists of a single UINT8 value:

| Status Code | Description |
| ----------- | ----------- |
| 0x00 | Idle |
| 0x01 | Command In Progress |
| 0x02 | Success |
| 0x03 | Failed |
| 0x04 | Invalid OpCode |
| 0x05 | Invalid Parameter |

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

[Espressif ESP32-C3 DevKitC-02 Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-devkitc-02/index.html)

[Espressif ESP32-S3 DevKitC-1 Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html)

[Waveshare ESP32-C3-Zero Board](https://www.waveshare.com/wiki/ESP32-C3-Zero)

[Waveshare ESP32-S3-Zero Board](https://www.waveshare.com/wiki/ESP32-S3-Zero)



