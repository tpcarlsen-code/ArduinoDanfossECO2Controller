#include "danfoss_api.h"
#include "ble.h"
#include "crypt.h"

Thermostat::Thermostat()
{
    this->_connected = false;
    this->_lastRead = 0;
    this->_rssi = 0;
    this->_batteryLevel = 0;
}

Thermostat::Thermostat(const char *friendlyName, const char *address, const char *key)
{
    strcpy(_friendlyName, friendlyName);
    strcpy(_address, address);
    strcpy(_key, key);
    this->_connected = false;
    this->_lastRead = 0;
    this->_rssi = 0;
    this->_batteryLevel = 0;
}

long Thermostat::lastRead()
{
    return this->_lastRead;
}

float Thermostat::desiredTemperature()
{
    return this->_temps.desired;
}

float Thermostat::targetTemperature()
{
    return this->_temps.set;
}

float Thermostat::measuredTemperature()
{
    return this->_temps.measured;
}

int Thermostat::batteryLevel()
{
    return this->_batteryLevel;
}

int Thermostat::rssi()
{
    return this->_rssi;
}

int Thermostat::wasConnected()
{
    return this->_lastConnectSuccess ? 1 : 0;
}

char *Thermostat::address()
{
    return this->_address;
}

char *Thermostat::friendlyName()
{
    return this->_friendlyName;
}

int Thermostat::connect()
{
    if (this->_connected)
    {
        return 1;
    }
    BLEDevice device = ble_connect(_address, 45);
    if (!device || !device.connected())
    {
        Serial.println(F("Could not connect!"));
        this->_lastConnectSuccess = false;
        return 0;
    }
    this->_bleDevice = device;
    this->_connected = true;
    this->_lastConnectSuccess = true;
    return 1;
}

int Thermostat::disconnect()
{
    this->_connected = false;
    return this->_bleDevice.disconnect() == true;
}

int Thermostat::sendPin(uint8_t pin[4])
{
    return ble_write_characteristic(this->_bleDevice, MAIN_SERVICE_ID, PIN_ID, pin, 4);
}

int Thermostat::sendEmptyPin()
{
    uint8_t pin[] = {0, 0, 0, 0};
    return sendPin(pin);
}

int Thermostat::read()
{
    if (!this->_connected)
    {
        Serial.println(F("Can not read thermostat. Not conncted!"));
        return 0;
    }
    uint8_t temperatureData[8];
    Temperatures t;
    t.valid = false;

    long start = millis();
    sendEmptyPin();
    int read = ble_read_characteristic(this->_bleDevice, MAIN_SERVICE_ID, TEMPERATURE_ID, temperatureData);
    while (read != 8 && (millis() - start > 10000))
    {
        Serial.print(F("Received unusual length of temperature data: "));
        Serial.println(read);
        read = ble_read_characteristic(this->_bleDevice, MAIN_SERVICE_ID, TEMPERATURE_ID, temperatureData);
    }
    if (read != 8)
    {
        return 0;
    }
    decrypt(temperatureData, 8, _key);
    this->_temps.valid = true;
    this->_temps.set = float(temperatureData[0] / 2.0);
    this->_temps.measured = float(temperatureData[1] / 2.0);

    uint8_t batteryData[1];
    sendEmptyPin();
    start = millis();
    read = ble_read_characteristic(this->_bleDevice, BATTERY_SERVICE, BATTERY_ID, batteryData);
    while (read != 1 && (millis() - start < 10000))
    {
        Serial.print(F("Received unusual length of battery data: "));
        Serial.println(read);
        read = ble_read_characteristic(this->_bleDevice, BATTERY_SERVICE, BATTERY_ID, batteryData);
    }
    if (read != 1)
    {
        return 0;
    }
    this->_batteryLevel = (int)*batteryData;

    int rssi = _bleDevice.rssi();
    if (rssi < 0)
    {
        this->_rssi = rssi;
    }

    this->_lastRead = millis();
    return 1;
}

int Thermostat::shouldBeUpdated()
{
    if (this->desiredTemperature() != this->targetTemperature() &&
        this->desiredTemperature() > 1.0 && this->targetTemperature() > 1.0)
    {
        return 1;
    }
    return 0;
}

int Thermostat::registerTargetTemperature(float target)
{
    this->_temps.desired = target;
}

int Thermostat::write()
{
    if (!this->_connected)
    {
        Serial.println(F("Can not write to thermostat. Not conncted!"));
        return 0;
    }
    uint8_t setTemperatureData[8] = {0};
    setTemperatureData[0] = _temps.desired * 2;
    sendEmptyPin();
    encrypt(setTemperatureData, 8, this->_key);
    int res = ble_write_characteristic(this->_bleDevice, MAIN_SERVICE_ID, TEMPERATURE_ID, setTemperatureData, 8);
    if (res == 1)
    {
        this->_temps.set = _temps.desired;
    }
    return res;
}

int Thermostat::readBatteryLevel()
{
    uint8_t batteryData[1];
    sendEmptyPin();
    long start = millis();
    int read = ble_read_characteristic(this->_bleDevice, BATTERY_SERVICE, BATTERY_ID, batteryData);
    while (read != 1 && (millis() - start < 10000))
    {
        Serial.print(F("Received unusual length of battery data: "));
        Serial.println(read);
        read = ble_read_characteristic(this->_bleDevice, BATTERY_SERVICE, BATTERY_ID, batteryData);
    }
    if (read != 1)
    {
        Serial.println("Could not read battery data!");
        return 0;
    }
    Serial.println(batteryData[0]);
    this->_batteryLevel = (int)*batteryData;
    return 1;
}