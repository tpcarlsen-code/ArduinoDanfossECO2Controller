#ifndef _BLE_H_
#define _BLE_H_
#include <ArduinoBLE.h>

BLEDevice ble_connect(const char *address, int timeout = 10);
int ble_read_characteristic(BLEDevice device, const char *serviceID, const char *characteristicID, uint8_t *result);
int ble_write_characteristic(BLEDevice device, const char *serviceID, const char *characteristicID, uint8_t *data, int length);

#endif