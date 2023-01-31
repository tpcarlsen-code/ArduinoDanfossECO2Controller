#ifndef _DANFOSS_API_H_
#define _DANFOSS_API_H_

#include <ArduinoBLE.h>

const char MAIN_SERVICE_ID[] = "10020000-2749-0001-0000-00805F9B042F";
const char KEY_ID[] = "1002000B-2749-0001-0000-00805F9B042F";
const char PIN_ID[] = "10020001-2749-0001-0000-00805F9B042F";
const char TEMPERATURE_ID[] = "10020005-2749-0001-0000-00805F9B042F";

const char BATTERY_SERVICE[] = "180F";
const char BATTERY_ID[] = "2A19";

struct Temperatures
{
    float measured;
    float set;
    float desired;
    bool valid;
};

struct Thermostat
{
    char friendlyName[25];
    char address[20];
    char key[40];
    Temperatures temps;
    int batteryLevel;
    int rssi;
    long lastRead;
    int connected;
};

int sendPin(BLEDevice d, uint8_t pin[4]);
int sendEmptyPin(BLEDevice d);
Temperatures readTemperatureData(BLEDevice d, const char *key);
bool setTargetTemperature(BLEDevice device, const char *key, float target);
int readBatteryLevel(BLEDevice device);

#endif