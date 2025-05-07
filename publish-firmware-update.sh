#!/bin/bash

# This shell script is used to publish a newly built firmware image
# onto the OTA update server.

# Use the value of CONFIG_RGB_LED_GPIO to figure out the target
# ESP32 platform.

RGB_LED_GPIO=$(grep -oP '(?<=#define CONFIG_RGB_LED_GPIO )\d+' build/config/sdkconfig.h)

# Check if the value was found
if [[ -z "$RGB_LED_GPIO" ]]; then
    echo "ERROR: CONFIG_RGB_LED_GPIO not found in sdkconfig.h"
    exit 1
fi

# Perform actions based on the value
case $RGB_LED_GPIO in
    8)
        echo "Target is Espressif ESP32-C3"
        TGT_DIR="./firmware_update_server/esp32c3"
        ;;
    10)
        echo "Target is Waveshare ESP32-C3-Zero"
        TGT_DIR="./firmware_update_server/esp32c3-zero"
        ;;
    21)
        echo "Target is Waveshare ESP32-S3-Zero"
        TGT_DIR="./firmware_update_server/esp32s3-zero"
        ;;
    48)
        echo "Target is Espressif ESP32-S3"
        TGT_DIR="./firmware_update_server/esp32s3"
        ;;
    *)
        echo "ERROR: Unsupported value for CONFIG_RGB_LED_GPIO"
        exit 1
        ;;
esac

mkdir -p $TGT_DIR
cp build/esp32SkelApp.bin $TGT_DIR/update.bin;
cp version.txt $TGT_DIR/version.txt;

exit 0
