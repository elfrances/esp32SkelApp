# Introduction

**esp32SkelApp** (or **SkelApp** for short) is an ESP-IDF skeletal app for the ESP32 SoC that can be used as a template when creating a new app that uses WiFi and/or BLE.
The optional features are configured using the "Skeletal App Configuration" section in the sdkconfig, and include:

### RGB LED

Provides an API for controling the color and mode of the RGB LED, to indicate relevant system events or states.  The supported colors are: black, blue, cyan, green, magenta, orange, purple, red, white, yellow.  The supporting modes are: solid on or blinking at 1, 2, or 4 times per second.

The table below describes a few uses of the RGB LED by the SkelApp":

| State | Color | Mode |
| ------ | ------- | ---------- | 
| Connecting to WiFi | Blue | 4 bps |
| WiFi Connection Established | Blue | solid |
| Invalid WiFi Credentials | Magenta | 4 bps |
| Inbound BLE Connection Established | Yelow | solid |
| OTA Update In Progress | Cyan | 4 bps |
| OTA Update Failure | Red | 4 bps |
| Fatal Error | Red | solid |

### Message Logging

Provides an API for logging messages to the console or to a file on the flash FAT FS.

### BLE Peripheral

Adds support for BLE peripheral functionality, so that an external BLE central can discover and connect to the ESP32-C3 device to configure it.

### WiFi Station

Adds support for WiFi station, so that the ESP32-C3 device can connect to a WiFi network to get Internet connectivity.

### OTA Update

Adds support for doing OTA firmware updates over WiFi.

### FAT File System

Creates a FAT file system using the 'storage' partition in the flash memory.


# How to use SkelApp

> [!NOTE]
> This document assumes the user is familiar with the typical workflow of building an ESP-IDF project, either using the command line tools from a terminal, or using the ESP-IDF plug-in with the VScode or Eclipse IDE's.

Let's assume the app your are developing is called "myNewApp".

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

4. Use the sample sdkconfig.esp32c3 file to create the project's sdkconfig file:

```
cp sdkconfig.esp32c3 sdkconfig
```

5. Run the ESP-IDF menuconfig command and select the desired options in the "Skeletal App Configuration" section, which comes right after the "Partition Table" setting:

```
idf.py menuconfig
```

> [!IMPORTANT]
> Some of the settings in the "Skeletal App Configuration" section depend on ESP-IDF "Component config" settings. For example, you won't be able to enable the BLE Peripheral setting unless the Bluetooth feature is enabled.

6. Add your own app's code to the appMainTask() in myNewApp/main/app.c.  This task runs a simple infinite work loop, with the period specified by the config attribute MAIN_TASK_WAKEUP_PERIOD.


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
| 0x05   | Set MLOG Destination | {UINT8: 0=NONE, 1=INFO, 2=TRACE, 3=DEBUG} |
| 0x06   | Set UTC Time | {UINT32: # seconds since the Epoch }
| 0x07   | Set UTC Offset | {INT8: # hours east or west from GMT } |
| 0x08   | WiFi | {UINT8: 0=Disabled, 1=Enabled}
| 0x09   | Dump MLOG File | none

For example, to set the UTC Time to May 2, 2025, 13:10 the command request bytes would be: 05 67 F8 D9 B8.

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



