# esp32SkelApp

A skeletal app for the ESP32 that can be used as a template when creating a new app that uses WiFi and/or BLE.
Most of the optional features are configured using the "Skeletal App Configuration" section in the sdkconfig.

# How to use it

Let's assume the app your are developing is called "myApp".

Clone this repo, and rename the top level directory "esp32SkelApp" to "myApp".

Edit the top-level CMakeLists.txt file and replace "esp32SkelApp" for "myApp", in the "project()" statement.

Run menuconfig and select the desired options in the "Skeletal App Configuration" section, which comes right after the "Partition Table" setting.

Add your app's code to the appMainTask() in myApp/main/app.c.
