# Arduino Danfoss ECO2 MQTT & Home Assistant integration

## What does it do?

Connects to Danfoss ECO2 radiator themostats. Reads & sets status to MQTT broker. Additionally pushes metris to Prometheus gateway.

## Configuration
Copy the file `common.example.h` to `common.h` and set your values. 

## Important
* Version of ArduinoBLE library must be 1.1.3. From version 1.2.0 and up connection with Danfoss ECO2 is not working. 
* Command messages from Home Assistant are set with retain flag. This is necessary because the Arduino devices with NINAW102 can not run BLE & WiFi at the same time meaning WiFi and BLE is connected & disconnected by demand. 