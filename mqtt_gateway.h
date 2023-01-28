#ifndef _MQTT_GATEWAY_H_
#define _MQTT_GATEWAY_H_

#include "danfoss_api.h"

bool gwGetThermostats(IPAddress gatewayIP, int gatewayPort, const char *arduinoID, Thermostat *out, int *count);
bool gwSendStatus(IPAddress gatewayIP, int gatewayPort, Thermostat *thermostats, int count);

#endif