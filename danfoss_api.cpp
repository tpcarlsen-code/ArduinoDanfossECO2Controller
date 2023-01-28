#include "danfoss_api.h"
#include "ble.h"
#include "crypt.h"

int sendPin(BLEDevice device, uint8_t pin[4])
{
    return writeCharacteristic(device, MAIN_SERVICE_ID, PIN_ID, pin, 4);
}

int sendEmptyPin(BLEDevice device)
{
    uint8_t pin[] = {0, 0, 0, 0};
    return sendPin(device, pin);
}

Temperatures readTemperatureData(BLEDevice device, const char *key)
{
    uint8_t temperatureData[8];
    Temperatures t;
    t.valid = false;

    sendEmptyPin(device);
    int read = readCharacteristic(device, MAIN_SERVICE_ID, TEMPERATURE_ID, temperatureData);
    if (read != 8)
    {
        Serial.print(F("Received unusual length of temperature data: "));
        Serial.println(read);
        return t;
    }
    decrypt(temperatureData, 8, key);
    t.valid = true;
    t.set = float(temperatureData[0] / 2.0);
    t.measured = float(temperatureData[1] / 2.0);
    return t;
}

bool setTargetTemperature(BLEDevice device, const char *key, float target)
{
    uint8_t setTemperatureData[8];
    setTemperatureData[0] = target * 2;
    setTemperatureData[1] = 0;
    sendEmptyPin(device);
    encrypt(setTemperatureData, 8, key);
    return writeCharacteristic(device, MAIN_SERVICE_ID, TEMPERATURE_ID, setTemperatureData, 8) == 1;
}

int readBatteryLevel(BLEDevice device)
{
    uint8_t batteryData[1];
    sendEmptyPin(device);
    int read = readCharacteristic(device, BATTERY_SERVICE, BATTERY_ID, batteryData);
    if (read != 1)
    {
        Serial.print(F("Received unusaul length of battery data: "));
        Serial.println(read);
        return -1;
    }
    return (int)*batteryData;
}