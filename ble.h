#ifndef _BLE_H_
#define _BLE_H_
#include <ArduinoBLE.h>

BLEDevice connect(const char *address, int timeout = 10);
int readCharacteristic(BLEDevice device, const char *serviceID, const char *characteristicID, uint8_t *result);
int writeCharacteristic(BLEDevice device, const char *serviceID, const char *characteristicID, uint8_t *data, int length);

#endif