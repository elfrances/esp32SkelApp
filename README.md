# esp32SkelApp

An ESP-IDF skeletal app for the ESP32 SoC that can be used as a template when creating a new app that uses WiFi and/or BLE.
Most of the optional features are configured using the "Skeletal App Configuration" section in the sdkconfig.

# How to use it

Let's assume the app your are developing is called "myApp".

```
git clone https://github.com/elfrances/esp32SkelApp.git
rm -rf esp32SkelApp/.git
mv esp32SkelApp myApp
sed -i s'/esp32SkelApp/myApp/' myApp/CMakeLists.txt
```

Run the ESP-IDF menuconfig command and select the desired options in the "Skeletal App Configuration" section, which comes right after the "Partition Table" setting.

Add your app's code to the appMainTask() in myApp/main/app.c.
