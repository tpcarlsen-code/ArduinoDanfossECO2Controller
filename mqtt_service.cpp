#include "mqtt_service.h"
#include <MqttClient.h>
#include <MemoryFree.h>
#include <WiFiNINA.h>

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char deviceClass[] = "danfoss_eco2";

/**
 * This block is used for reading messages from broker.
 * We need to set some variables to current state as
 * callback function is not aware of context.
 */
float currentTargetTemperature;
MqttClient *currentMqttClient;
void readMessage(int size)
{
    char val[32];
    mqttClient.readBytes(val, size);
    val[size] = '\0';
    Serial.print(F("Received message: "));
    Serial.println(val);

    float f = atoff(val);
    currentTargetTemperature = f;
}

MQTTService::MQTTService(const char *b, int p)
{
    broker = b;
    port = p;
}

int MQTTService::registerThermostats(Thermostat *thermostats, int numThermostats)
{
    if (!mqttClient.connect(broker, port))
    {
        Serial.print(F("MQTT connection failed! Error code = "));
        Serial.println(mqttClient.connectError());
        return 0;
    }
    char topic[128];
    char message[1024];

    Serial.print("Auto registering: ");
    Serial.println(numThermostats);
    for (int i = 0; i < numThermostats; i++)
    {
        composeAutoDiscoveryTopic(topic, "climate", "thermostat", thermostats[i]);
        composeAutoDiscoveryClimateMessage(message, thermostats[i]);

        Serial.println(topic);
        Serial.println(message);
        Serial.println();
        mqttClient.beginMessage(topic, true);
        mqttClient.print(message);
        mqttClient.endMessage();

        composeAutoDiscoveryTopic(topic, "sensor", "temperature", thermostats[i]);
        composeAutoDiscoverySensorTemperatureMessage(message, thermostats[i]);

        Serial.println(topic);
        Serial.println(message);
        Serial.println();
        mqttClient.beginMessage(topic, true);
        mqttClient.print(message);
        mqttClient.endMessage();

        composeAutoDiscoveryTopic(topic, "sensor", "battery", thermostats[i]);
        composeAutoDiscoverySensorBatteryMessage(message, thermostats[i]);

        Serial.println(topic);
        Serial.println(message);
        Serial.println();
        mqttClient.beginMessage(topic, true);
        mqttClient.print(message);
        mqttClient.endMessage();

        mqttClient.beginMessage("danfoss_eco2/state", true);
        mqttClient.print("online");
        mqttClient.endMessage();
    }

    mqttClient.stop();
    return 1;
}

int MQTTService::pushSensorData(Thermostat thermostat)
{
    if (!mqttClient.connect(broker, port))
    {
        Serial.print(F("MQTT connection failed! Error code = "));
        Serial.println(mqttClient.connectError());
        return 0;
    }
    char topic[128];
    char message[512];

    composeStateTopic(topic, thermostat);
    composeStateMessage(message, thermostat);

    mqttClient.beginMessage(topic);
    mqttClient.print(message);
    mqttClient.endMessage();

    mqttClient.stop();
    return 1;
}

int MQTTService::getUpdates(Thermostat *thermostats, int numThermostats)
{
    if (!mqttClient.connect(broker, port))
    {
        Serial.print(F("MQTT connection failed! Error code = "));
        Serial.println(mqttClient.connectError());
        return 0;
    }
    char subscribeTopic[64];
    //currentThermostats = thermostats;
    currentMqttClient = &mqttClient;
    for (int i = 0; i < numThermostats; i++)
    {
        currentTargetTemperature = 0.0;
        sprintf(subscribeTopic, "%s/%s/set", deviceClass, thermostats[i].friendlyName());
        mqttClient.onMessage(readMessage);
        mqttClient.subscribe(subscribeTopic);
        delay(500);
        mqttClient.unsubscribe(subscribeTopic);
        if (currentTargetTemperature > 0) {
            thermostats[i].registerTargetTemperature(currentTargetTemperature);
        }
    }
    mqttClient.stop();
    return 1;
}

void MQTTService::composeAutoDiscoveryTopic(char *out, const char *haasClass, const char *type, Thermostat thermostat)
{
    char name[64];
    sprintf(name, "%s_%s", thermostat.friendlyName(), type);
    sprintf(out, "homeassistant/%s/%s/%s/config", haasClass, deviceClass, name);
}

void MQTTService::composeStateTopic(char *out, Thermostat thermostat)
{
    sprintf(out, "%s/%s/state", deviceClass, thermostat.friendlyName());
}

void MQTTService::composeAutoDiscoveryClimateMessage(char *out, Thermostat thermostat)
{
    char message[] = "{\"~\": \"%s/%s\","
                     "\"name\": \"%s Thermostat\","
                     "\"unique_id\": \"%s_thermostat\","
                     "\"temp_cmd_t\": \"~/set\","
                     "\"temp_stat_t\": \"~/state\","
                     "\"temp_stat_tpl\": \"{{ value_json.set_point }}\","
                     "\"curr_temp_t\": \"~/state\","
                     "\"curr_temp_tpl\": \"{{ value_json.room_temp }}\","
                     "\"min_temp\": \"12\","
                     "\"max_temp\": \"26\","
                     "\"temp_step\": \"0.5\","
                     "\"modes\":[\"auto\"],"
                     "\"retain\": true,"
                     "\"device\": {"
                     "\"identifiers\": \"%s\","
                     "\"manufacturer\": \"Danfoss\","
                     "\"model\": \"ECO2\","
                     "\"name\": \"%s\""
                     "},"
                     "\"availability_topic\": \"%s/state\","
                     "\"payload_available\": \"online\","
                     "\"payload_not_available\": \"offline\""
                     "}";
    sprintf(out, message,
            deviceClass, thermostat.friendlyName(), thermostat.friendlyName(), thermostat.friendlyName(),
            thermostat.friendlyName(), thermostat.friendlyName(), deviceClass);
}

void MQTTService::composeAutoDiscoverySensorBatteryMessage(char *out, Thermostat thermostat)
{
    char message[] = "{\"device_class\": \"battery\", \"name\": \"%s Battery\", \"unique_id\": \"%s_battery\","
                     "\"state_topic\": \"%s/%s/state\", \"value_template\": \"{{ value_json.battery }}\","
                     "\"unit_of_measurement\": \"%\", \"device\": {\"identifiers\": \"%s\", \"manufacturer\": \"Danfoss\","
                     "\"model\": \"ECO2\", \"name\": \"%s\"}, \"availability_topic\": \"%s/state\", \"payload_available\":"
                     "\"online\", \"payload_not_available\": \"offline\"}";
    sprintf(out, message, thermostat.friendlyName(), thermostat.friendlyName(), deviceClass,
            thermostat.friendlyName(), thermostat.friendlyName(), thermostat.friendlyName(), deviceClass);
}

void MQTTService::composeAutoDiscoverySensorTemperatureMessage(char *out, Thermostat thermostat)
{
    char message[] = "{\"device_class\": \"temperature\", \"name\": \"%s Temperature\","
                     "\"unique_id\": \"%s_temperature\", \"state_topic\": \"%s/%s/state\","
                     "\"value_template\": \"{{ value_json.room_temp }}\", \"unit_of_measurement\": \"\u00b0C\","
                     "\"device\": {\"identifiers\": \"%s\", \"manufacturer\": \"Danfoss\","
                     "\"model\": \"ECO2\", \"name\": \"%s\"}, \"availability_topic\": \"%s/state\","
                     "\"payload_available\": \"online\", \"payload_not_available\": \"offline\"}";
    sprintf(out, message, thermostat.friendlyName(), thermostat.friendlyName(), deviceClass,
            thermostat.friendlyName(), thermostat.friendlyName(), thermostat.friendlyName(), deviceClass);
}

void MQTTService::composeStateMessage(char *out, Thermostat thermostat)
{
    char message[] = "{"
                     "\"name\": \"%s\","
                     "\"battery\": %d,"
                     "\"room_temp\": %.1f,"
                     "\"set_point\": %.1f"
                     //  "\"last_update\": \"%s\""
                     "}";
    sprintf(out, message, thermostat.friendlyName(),
            thermostat.batteryLevel(), thermostat.measuredTemperature(), thermostat.targetTemperature());
}

MQTTService::~MQTTService() {}
