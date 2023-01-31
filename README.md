# Arduino Danfoss ECO2 MQTT & Home Assistant integration

## What does it do?

## Configuration

## Important
* Version of ArduinoBLE library must be 1.1.3. From version 1.2.0 and up connection with Danfoss ECO2 is not working. 
* Command messages from Home Assistant are set with retain flag. This is necessary because the Arduino devices with NINAW102 can not run BLE & WiFi at the same time meaning WiFi and BLE is connected & disconnected by demand. 