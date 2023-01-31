#ifndef _HQTT_H_
#define _MQTT_H_
#include <MqttClient.h>
#include "danfoss_api.h"

class MQTTService
{
public:
  MQTTService(const char *broker, int port);
  virtual ~MQTTService();
  int registerThermostats(Thermostat *thermostats, int numThermostats);
  int pushSensorData(Thermostat thermostat);
  int getUpdates(Thermostat *thermostats, int numThermostats);

private:
  void composeAutoDiscoveryTopic(char *out, const char *haasClass, const char *type, Thermostat thermostat);
  void composeAutoDiscoveryClimateMessage(char *out, Thermostat thermostat);
  void composeAutoDiscoverySensorBatteryMessage(char *out, Thermostat thermostat);
  void composeAutoDiscoverySensorTemperatureMessage(char *out, Thermostat thermostat);
  void composeStateTopic(char *out, Thermostat thermostat);
  void composeStateMessage(char *out, Thermostat thermostat);

  const char *broker;
  int port;
};

#endif
/*
{
    "~": "danfoss_eco2/%s",
    "name": "%s Thermostat",
    "unique_id": "%s_thermostat",
    "temp_cmd_t": "~/set",
    "temp_stat_t": "~/state",
    "temp_stat_tpl": "{{ value_json.set_point }}",
    "curr_temp_t": "~/state",
    "curr_temp_tpl": "{{ value_json.room_temp }}",
    "min_temp": "10",
    "max_temp": "30",
    "temp_step": "0.5",
    "device": {
        "identifiers": "%s",
        "manufacturer": "Danfoss",
        "model": "ECO2",
        "name": "%s"
    },
    "availability_topic": "danfoss_eco2/state",
    "payload_available": "online",
    "payload_not_available": "offline"
}
*/

/*
danfoss_eco2/stue_syd/state
{
  "name": "Stue syd",
  "battery": 64,
  "room_temp": 21.0,
  "set_point": 21.5,
  "last_update": "2023-01-29T11:44:09.817597+01:00"
}
*/

/*
danfoss_eco2/stue_syd/set
21.5
*/