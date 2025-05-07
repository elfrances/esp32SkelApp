#!/bin/bash

# This shell script starts a simple HTTPS server to handle the OTA updates

openssl s_server -WWW -key ota_key.pem -cert ota_cert.pem -4 -port 8070
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to start OTA Update Server!"
fi

