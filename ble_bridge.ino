#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include "utility/wifi_drv.h"
#include <MemoryFree.h>
#include "ble.h"
#include "danfoss_api.h"
#include "mqtt_service.h"
#include "xxtea.h"
#include "env.h"
#include "thermostats.h"

int THERMOSTAT_STATUS_UPDATE_INTERVAL = 10 * 60 * 1000;
int MQTT_CHECK_INTERVAL = 30 * 1000;
int METRICS_PUSH_INTERVAL = 60 * 1000;
int MQTT_PING_INTERVAL = 5 * 60 * 1000;

long lastMQTTCheck = 0;
long lastMetricsPush = 0;
long lastMQTTPing = 0;

Thermostat thermostats[20]; // reserve space for 20 thermostats.
int numThermostats = 0;

char *messages[100];
int numMessages = 0;

char deviceID[30];

int dontUpdate = 0;

bool firstRun = true;

MQTTService mqtt(mqttBroker, mqttPort);

void (*resetFunc)(void) = 0; // declare reset function at address 0

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  sprintf(deviceID, "%02x_%02x_%02x_%02x_%02x_%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Initialize the buffers for xxtea.
  xxtea_init();

  parseThermostats(thermostatConfig);
  if (numThermostats == 0)
  {
    Serial.println("No devices configured!");
    while (1)
    {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(150);
      digitalWrite(LED_BUILTIN, LOW);
      delay(150);
    }
  }

  while (!doWithWiFi(taskSetMQTTAutoDiscovery, -1))
  {
    Serial.println(F("Set auto discovery on MQTT failed in setup. Retrying!"));
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }

  resetTimers();
}

void resetTimers()
{
  lastMQTTCheck = lastMetricsPush = lastMQTTPing = millis();
}

void loop()
{
  long now = millis();

  if (now - lastMetricsPush > METRICS_PUSH_INTERVAL)
  {
    lastMetricsPush = now;
    Serial.println(F("--- Performing metrics push task"));
    doWithWiFi(sendMetrics, 0);
    Serial.println(F("--- done\n"));
  }

  if (now - lastMQTTCheck > MQTT_CHECK_INTERVAL)
  {
    lastMQTTCheck = now;
    Serial.println(F("--- Performing MQTT update check task"));
    doWithWiFi(taskCheckMQTTForChanges, 0);
    Serial.println(F("--- done\n"));
  }

  for (int i = 0; i < numThermostats; i++)
  {
    if (now - thermostats[i].lastRead() > THERMOSTAT_STATUS_UPDATE_INTERVAL || thermostats[i].batteryLevel() == 0 || firstRun)
    {
      Serial.print(F("--- Performing thermostat status check task for "));
      Serial.println(thermostats[i].friendlyName());
      if (doWithBLE(&thermostats[i], readThermostat))
      {
        Serial.println(F("--- Starting status write"));
        doWithWiFi(taskSendStatusToMQTT, i);
        Serial.println(F("--- done status write"));
      }
      printThermostat(thermostats[i]);

      Serial.println(F("--- done\n"));
    }
  }

  for (int i = 0; i < numThermostats; i++)
  {
    if (thermostats[i].shouldBeUpdated() && !dontUpdate)
    {
      char updateMessage[100];
      sprintf(updateMessage,
              "Thermostat %s has %f should have %f",
              thermostats[i].friendlyName(),
              thermostats[i].targetTemperature(),
              thermostats[i].desiredTemperature());
      Serial.println(updateMessage);
      if (!doWithBLE(&thermostats[i], updateThermostat))
      {
        Serial.println(F("Update failed!"));
      }
      doWithWiFi(taskSendStatusToMQTT, i);
    }
  }
  firstRun = false;

  if (millis() > 10 * 60 * 1000)
  {
    Serial.println(F("Reset"));
    resetFunc();
  }
}

int readThermostat(Thermostat *t)
{
  if (!t->read())
  {
    Serial.println(F("Could not read!"));
    return 0;
  }
  return 1;
}

int updateThermostat(Thermostat *t)
{
  int res = t->write();
  Serial.println(F("--- done\n"));
  return res;
}

int taskSendStatusToMQTT(int thermostatIndex)
{
  if (!mqtt.pushSensorData(thermostats[thermostatIndex]))
  {
    Serial.println(F("Could not send status update."));
    return 0;
  }
  return 1;
}

int taskCheckMQTTForChanges(int notUsed)
{
  if (!mqtt.getUpdates(thermostats, numThermostats))
  {
    Serial.println(F("Could not get MQTT changes."));
    return 0;
  }
  return 1;
}

int sendMetrics(int notUsed)
{
  WiFiClient client;
  /*
  if (strlen(alivePing) > 0)
  {
    if (client.connect("192.168.1.52", 3001))
    {
      client.print("GET /api/push/dtJ2ZXazQu?status=up&msg=OK&ping= HTTP/1.1\r\n");
      client.print("Host: status.spikyhouse.com\r\n");
      client.print("Connection: close\r\n");
      client.print("\r\n");
      client.flush();
      client.stop();
    }
  }
  */

  if (client.connect(metricsServer, 9091))
  {
    char line1[60];
    sprintf(line1, "POST /metrics/job/%s HTTP/1.1\r\n", deviceID);

    char body[1024];
    sprintf(body, "memory_free_bytes %d\nconnected_thermostats %d\n", freeMemory(), activeThermostats());
    char thermostatMetrics[1024];
    const char tmpl[] = "thermostat_rssi{friendly_name=\"%s\"} %d\n"
                        "thermostat_battery_level{friendly_name=\"%s\"} %d\n"
                        "thermostat_connected{friendly_name=\"%s\"} %d\n"
                        "thermostat_room_temperature{friendly_name=\"%s\"} %.1f\n"
                        "thermostat_set_temperature{friendly_name=\"%s\"} %.1f\n";
    for (int i = 0; i < numThermostats; i++)
    {
      if (thermostats[i].batteryLevel() == 0)
      {
        continue; // Thermostat has no valid values.
      }
      sprintf(thermostatMetrics, tmpl,
              thermostats[i].friendlyName(), thermostats[i].rssi(),
              thermostats[i].friendlyName(), thermostats[i].batteryLevel(),
              thermostats[i].friendlyName(), thermostats[i].wasConnected(),
              thermostats[i].friendlyName(), thermostats[i].measuredTemperature(),
              thermostats[i].friendlyName(), thermostats[i].targetTemperature());
      strcat(body, thermostatMetrics);
    }

    client.print(line1);
    client.print("Connection: close\r\n");
    client.print("Content-Type: application/x-www-form-urlencoded\r\n");
    client.print("Content-Length: ");
    client.print(strlen(body));
    client.print("\r\n");
    client.print("Host: ");
    client.print(metricsServer);
    client.print(":9091\r\n");
    client.print("\r\n");
    client.print(body);
    client.flush();
    client.stop();
  }

  return 1;
}

int taskSetMQTTAutoDiscovery(int notUse)
{
  Serial.println(F("--- Starting MQTT write auto discovery"));
  if (!mqtt.registerThermostats(thermostats, numThermostats))
  {
    Serial.println(F("--- An error occurred."));
    return 0;
  }

  Serial.println(F("--- done\n"));
  return 1;
}

int doWithWiFi(int (*task)(int), int param)
{
  if (!connectWiFi())
  {
    Serial.println(F("--- Could not connect to WiFi. Exiting task."));
    return 0;
  }
  digitalWrite(LED_BUILTIN, HIGH);
  int value = task(param);
  WiFi.disconnect();
  WiFi.end();
  digitalWrite(LED_BUILTIN, LOW);
  return value;
}

int doWithBLE(Thermostat *t, int (*task)(Thermostat *))
{
  BLE.begin();
  int connected = t->connect();
  if (!connected)
  {
    Serial.print(F("Could not connect to "));
    Serial.println(t->address());
    t->disconnect();
    BLE.end();
    return 0;
  }
  int value = task(t);
  t->disconnect();
  BLE.end();
  return value;
}

bool connectWiFi()
{
  wiFiDrv.wifiDriverDeinit();
  wiFiDrv.wifiDriverInit();
  long start = millis();
  int status = WiFi.status();
  while (status != WL_CONNECTED && (millis() - start < 30000))
  {
    status = WiFi.begin(ssid, pass);
    delay(500);
    if (status == WL_CONNECTED)
    {
      return true;
    }
  }
  return false;
}

int thermostatNeedsUpdating()
{
  for (int i = 0; i < numThermostats; i++)
  {
    if (thermostats[i].shouldBeUpdated())
    {
      return i;
    }
  }
  return -1;
}

void printThermostat(Thermostat t)
{
  Serial.print(F("Friendly name: "));
  Serial.println(t.friendlyName());
  Serial.print(F("Address: "));
  Serial.println(t.address());
  //  Serial.print(F("Key: "));
  //  Serial.println(t.key());
  Serial.print(F("Set temperature: "));
  Serial.println(t.targetTemperature());
  Serial.print(F("Room temperature: "));
  Serial.println(t.measuredTemperature());
  Serial.print(F("Desired temperature: "));
  Serial.println(t.desiredTemperature());
  Serial.print(F("Battery: "));
  Serial.println(t.batteryLevel());
  Serial.print(F("RSSI: "));
  Serial.println(t.rssi());
  Serial.print(F("Last read: "));
  Serial.println(t.lastRead());
}

void parseThermostats(const char *body)
{
  int currentThermostatIndex = 0;
  int currentFieldIndex = 0;
  char currentFieldValue[50];
  int currentFieldPointer = 0;
  char friendlyName[50];
  char address[50];
  char key[50];

  for (int i = 0; i < strlen(body); i++)
  {
    if (body[i] == '\n' || body[i] == ' ')
    {
      currentFieldValue[currentFieldPointer] = '\0';
      switch (currentFieldIndex)
      {
      case 0:
        strcpy(friendlyName, currentFieldValue);
        break;
      case 1:
        strcpy(address, currentFieldValue);
        break;
      case 2:
        strcpy(key, currentFieldValue);
        break;
      }
      memset(currentFieldValue, 0, 50);
      currentFieldIndex++;
      currentFieldPointer = 0;
      if (body[i] == '\n')
      {
        Thermostat t(friendlyName, address, key);
        currentFieldIndex = 0;
        thermostats[currentThermostatIndex] = t;
        currentThermostatIndex++;
      }
    }
    else
    {
      currentFieldValue[currentFieldPointer++] = body[i];
    }
  }
  numThermostats = currentThermostatIndex;
}

int activeThermostats()
{
  int active = 0;
  for (int i = 0; i < numThermostats; i++)
  {
    if (thermostats[i].lastRead() > 0 && (thermostats[i].lastRead() + 20 * 60 * 1000) > millis())
    {
      active++;
    }
  }
  return active;
}