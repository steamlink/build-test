#!/usr/bin/env bash

# Starting mosquitto in the background
echo "*******************************"
echo "Starting Mosquitto at port 8883"
echo "*******************************"
/usr/local/Cellar/mosquitto/1.4.10/sbin/mosquitto -d -c /usr/local/etc/mosquitto/mosquitto.conf -p 8883
echo ""
echo ""
echo "*******************************"
echo "Following log below...         "
echo "*******************************"
tail -f /Users/udit/log/mosquitto.log
