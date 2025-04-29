# Introduction

**esp32SkelApp** is an ESP-IDF skeletal app for the ESP32 SoC that can be used as a template when creating a new app that uses WiFi and/or BLE.
Most of the optional features are configured using the "Skeletal App Configuration" section in the sdkconfig.

# How to use it

Let's assume the app your are developing is called "myNewApp".

```
git clone https://github.com/elfrances/esp32SkelApp.git
rm -rf esp32SkelApp/.git
mv esp32SkelApp myNewApp
sed -i s'/esp32SkelApp/myNewApp/' myNewApp/CMakeLists.txt
```

Run the ESP-IDF menuconfig command and select the desired options in the "Skeletal App Configuration" section, which comes right after the "Partition Table" setting.

Add your own app's code to the appMainTask() in myNewApp/main/app.c.  This task runs a simple infinite work loop, with the period specified by the config attribute MAIN_TASK_TICK_PERIOD.

# Example

Using the following SDK Configuration: 

![Skeletal-App-Configuration](/assets/Skeletal-App-Configuration.png) 

the firmware would print the following messages on the terminal when it boots up:

![Skeletal-App-Running](/assets/Skeletal-App-Running.png) 



