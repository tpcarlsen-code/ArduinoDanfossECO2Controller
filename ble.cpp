#include "ble.h"
#include <stdint.h>
#include "ArduinoBLE.h"

BLEDevice ble_connect(const char *address, int timeout)
{
    long start = millis();
    BLE.scanForAddress(address);
    while (millis() - start < timeout * 1000)
    {
        BLEDevice device = BLE.available();
        if (device)
        {
            Serial.println(F("Device found"));
            BLE.stopScan();
            delay(500);
            device.connect();
            delay(500);
            while (!device.connected() && (millis() - start < timeout * 1000))
            {
                delay(500);
                device.connect();
            }
            if (!device.connected())
            {
                Serial.println(F("Could not connect to device!"));
                return device;
            }
            device.discoverAttributes();
            return device;
        }
    }
    return BLEDevice();
}

int ble_read_characteristic(BLEDevice device, const char *serviceID, const char *characteristicID, uint8_t *result)
{
    BLEService service = device.service(serviceID);
    if (!service)
    {
        Serial.println(F("Could not find service!"));
        return 0;
    }
    BLECharacteristic characteristic = service.characteristic(characteristicID);
    if (!characteristic)
    {
        Serial.println(F("Could not find characteristic!"));
        return 0;
    }
    if (!characteristic.read())
    {
        Serial.println(F("Read fail!"));
    }
    return characteristic.readValue(result, characteristic.valueLength());
}

int ble_write_characteristic(BLEDevice device, const char *serviceID, const char *characteristicID, uint8_t *data, int length)
{
    BLEService service = device.service(serviceID);
    if (!service)
    {
        Serial.println(F("service not found!"));
        return 0;
    }
    BLECharacteristic c = service.characteristic(characteristicID);
    if (!c)
    {
        Serial.println(F("characteristic not found!"));
        return 0;
    }
    return c.writeValue(data, length);
}
