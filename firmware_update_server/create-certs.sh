#!/bin/bash

# This interactive OpenSSL command is used to generate the private/public key 
# pair used by the HTTPS OTA server. See the example below:
# 
# 
# $ openssl req -x509 -newkey rsa:2048 -keyout ota_key.pem -out ota_cert.pem -days 3650 -nodes
# .....+...+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*..+.........+......+.+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*.+....+......+.....+.........+......+.........+..........+..+.....................+.......+...............+...+..+.+...............+.................+.......+...+..+......+.+...+...........+...................+.....+...+....+...+...+..+......+..............................+......+.........+...............+......+.+..+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# ...+.+...+........+...+....+...............+........+...+.........+.+..+...+.+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*.+.+......+...+..+..........+........+.+......+..+.+.....+......+...+..........+......+..+..........+.....+...............+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*........+.............+.........+......+.........+......+......+........+.+......+.....+.+............+........+..................+......+...+.......+...+.....+.......+...........+...+...............+.+......+..............+.............+...+...+..+...+......+..........+.........+.........+.....+.+......+...+......+.................+...................+...+...........+....+...+.........+.....+...+...+.......+...+...+.....+..........+.....+.......+..+...+....+...+..+.........+.+...+..+......+........................+.........+.......+...+.........+..+.+..+.+...........+.........+......+.+........+...............+.......+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# -----
# You are about to be asked to enter information that will be incorporated
# into your certificate request.
# What you are about to enter is what is called a Distinguished Name or a DN.
# There are quite a few fields but you can leave some blank
# For some fields there will be a default value,
# If you enter '.', the field will be left blank.
# -----
# Country Name (2 letter code) [AU]:US
# State or Province Name (full name) [Some-State]:Arizona
# Locality Name (eg, city) []:North Rim
# Organization Name (eg, company) [Internet Widgits Pty Ltd]:Acme Corporation
# Organizational Unit Name (eg, section) []:Tech Support
# Common Name (e.g. server FQDN or YOUR name) []:ota.acme.com
# Email Address []:wilie@acme.com
# 
# Once you have generated the private key and the public certificate, the file
# ota_cert.pem must be copied to <proj-dir>/server_certs/ota_cert.pem and the 
# firmware image rebuilt.
# 
# $ cp ota_cert.pem ../server_certs/
# $ cd ..
# $ idf.py build
# $ idf.py flash -p /dev/ttyUSB0
 
openssl req -x509 -newkey rsa:2048 -keyout ota_key.pem -out ota_cert.pem -days 3650 -nodes
if [ $? -eq 0 ]; then
    echo ""
    echo "NOTE: Next, you need to copy the certificate ota_cert.pen to the project's server_certs directory, and rebuild the firmware image..."
    echo ""
else
    echo "Failed to create key/cert pair!"
fi

