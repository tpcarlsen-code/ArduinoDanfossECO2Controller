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

class Thermostat
{
public:
    Thermostat();
    Thermostat(const char *friendlyName, const char *address, const char *key);
    int connect();
    int disconnect();
    int read();
    int registerTargetTemperature(float target);
    int shouldBeUpdated();
    int write();
    long lastRead();
    float desiredTemperature();
    float targetTemperature();
    float measuredTemperature();
    int batteryLevel();
    int rssi();
    int wasConnected();
    unsigned long connectTime();
    char *address();
    char *friendlyName();

private:
    int sendPin(uint8_t pin[4]);
    int sendEmptyPin();
    int readBatteryLevel();

    BLEDevice _bleDevice;
    bool _connected;
    unsigned long _connectTime;
    bool _lastConnectSuccess;
    char _friendlyName[25];
    char _address[18]; // mac address + \0
    char _key[33];     // 32 char hex string + \0
    Temperatures _temps;
    int _batteryLevel;
    int _rssi;
    long _lastRead;
    float tempCorrection;
};

#endif