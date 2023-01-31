#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include "utility/wifi_drv.h"
#include <MemoryFree.h>
#include "crypt.h"
#include "ble.h"
#include "danfoss_api.h"
#include "xxtea.h"
#include "env.h"
#include "thermostats.h"
#include "mqtt_service.h"

int THERMOSTAT_STATUS_UPDATE_INTERVAL = 10 * 60 * 1000;
int MQTT_CHECK_INTERVAL = 30 * 1000;
int METRICS_PUSH_INTERVAL = 30 * 1000;
int MQTT_PING_INTERVAL = 5 * 60 * 1000;

long lastMQTTCheck = 0;
long lastMetricsPush = 0;
long lastMQTTPing = 0;

Thermostat thermostats[10];
int numThermostats = 0;

char *errors[100];

char deviceID[30];

int dontUpdate = 0;

bool firstRun = true;

MQTTService mqtt(mqttBroker, mqttPort);

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);
  delay(2500);

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

  while (!doWithWiFi(taskSetMQTTAutoDiscovery, 0))
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
    Serial.println(F("--- Performing metrics task"));
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
    if (now - thermostats[i].lastRead > THERMOSTAT_STATUS_UPDATE_INTERVAL || firstRun)
    {
      Serial.print(F("--- Performing thermostat status check task for "));
      Serial.println(thermostats[i].friendlyName);
      if (readThermostat(i))
      {
        Serial.println(F("--- Starting status write"));
        doWithWiFi(taskSendStatusToMQTT, i);
        Serial.println(F("--- done status write"));
        thermostats[i].lastRead = now;
        thermostats[i].connected = 1;
      }
      else
      {
        thermostats[i].connected = 0;
        thermostats[i].lastRead = (now - THERMOSTAT_STATUS_UPDATE_INTERVAL) + 3000;
      }
      Serial.println(F("--- done\n"));
    }
  }

  int shouldBeUpdated = thermostatNeedsUpdating();
  if (shouldBeUpdated > -1 && !dontUpdate)
  {
    char updateMessage[100];
    sprintf(updateMessage,
            "Thermostat %s has %f should have %f",
            thermostats[shouldBeUpdated].friendlyName,
            thermostats[shouldBeUpdated].temps.set,
            thermostats[shouldBeUpdated].temps.desired);
    Serial.println(updateMessage);
    if (updateThermostat(shouldBeUpdated))
    {
      thermostats[shouldBeUpdated].temps.set = thermostats[shouldBeUpdated].temps.desired;
    }
    doWithWiFi(taskSendStatusToMQTT, shouldBeUpdated);
  }
  firstRun = false;
}

/**
 * Obsolete
int taskReadThermostats()
{
  connectedThermostats = 0;
  if (numThermostats == 0)
  {
    Serial.println(F("--- No thermostats registered. Skipping."));
    return 0;
  }
  BLE.begin();
  for (int i = 0; i < numThermostats; i++)
  {
    if (!readThermostat(i))
    {
      Serial.println(F("Could not read themostat."));
    }
    else
    {
      connectedThermostats++;
    }
  }
  BLE.end();
  return 1;
}
*/

int readThermostat(int index)
{
  BLE.begin();
  BLEDevice thermostatBLE = connect(thermostats[index].address, 30);
  if (!thermostatBLE || !thermostatBLE.connected())
  {
    Serial.print(F("Could not connect to "));
    Serial.println(thermostats[index].address);
    BLE.end();
    return 0;
  }
  int rssi = thermostatBLE.rssi();
  if (rssi < 0)
  {
    thermostats[index].rssi = rssi;
  }
  int batteryLevel = readBatteryLevel(thermostatBLE);
  if (batteryLevel > -1)
  {
    thermostats[index].batteryLevel = batteryLevel;
  }
  Temperatures temps = readTemperatureData(thermostatBLE, thermostats[index].key);
  thermostatBLE.disconnect();
  if (temps.valid)
  {
    thermostats[index].temps.measured = temps.measured;
    thermostats[index].temps.set = temps.set;
  }
  printThermostat(thermostats[index]);
  thermostatBLE.disconnect();
  BLE.end();
  return 1;
}

int updateThermostat(int index)
{
  BLE.begin();
  Serial.println(F("--- Performing thermostat update"));
  BLEDevice thermostatBLE = connect(thermostats[index].address, 30);
  if (!thermostatBLE || !thermostatBLE.connected())
  {
    Serial.print(F("Could not connect to "));
    Serial.println(thermostats[index].address);
    thermostatBLE.disconnect();
    return 0;
  }
  if (!setTargetTemperature(thermostatBLE, thermostats[index].key, thermostats[index].temps.desired))
  {
    Serial.print(F("Could not update!"));
    thermostatBLE.disconnect();
    BLE.end();
    return 0;
  }

  thermostatBLE.disconnect();
  BLE.end();
  Serial.println(F("--- done\n"));
  return 1;
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

int taskCheckMQTTForChanges(int num)
{
  if (!mqtt.getUpdates(thermostats, numThermostats))
  {
    Serial.println(F("Could not get MQTT changes."));
    return 0;
  }
  return 1;
}

int sendMetrics(int num)
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
      if (thermostats[i].batteryLevel == 0) {
        continue;
      }
      sprintf(thermostatMetrics, tmpl,
              thermostats[i].friendlyName, thermostats[i].rssi,
              thermostats[i].friendlyName, thermostats[i].batteryLevel,
              thermostats[i].friendlyName, thermostats[i].connected,
              thermostats[i].friendlyName, thermostats[i].temps.measured,
              thermostats[i].friendlyName, thermostats[i].temps.set);
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

  Serial.println(F("--- done"));
  Serial.println();
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
    if (thermostats[i].temps.desired != thermostats[i].temps.set &&
        thermostats[i].temps.set > 0 && thermostats[i].temps.desired > 0)
    {
      return i;
    }
  }
  return -1;
}

void printThermostat(Thermostat t)
{
  Serial.print(F("Friendly name: "));
  Serial.println(t.friendlyName);
  Serial.print(F("Address: "));
  Serial.println(t.address);
  Serial.print(F("Key: "));
  Serial.println(t.key);
  Serial.print(F("Set temperature: "));
  Serial.println(t.temps.set);
  Serial.print(F("Room temperature: "));
  Serial.println(t.temps.measured);
  Serial.print(F("Desired temperature: "));
  Serial.println(t.temps.desired);
  Serial.print(F("Battery: "));
  Serial.println(t.batteryLevel);
  Serial.print(F("RSSI: "));
  Serial.println(t.rssi);
}

void parseThermostats(const char *body)
{
  int currentThermostatIndex = 0;
  int currentFieldIndex = 0;
  char currentFieldValue[50];
  int currentFieldPointer = 0;
  for (int i = 0; i < strlen(body); i++)
  {
    if (body[i] == '\n' || body[i] == ' ')
    {
      currentFieldValue[currentFieldPointer] = '\0';
      switch (currentFieldIndex)
      {
      case 0:
        strcpy(thermostats[currentThermostatIndex].friendlyName, currentFieldValue);
        break;
      case 1:
        strcpy(thermostats[currentThermostatIndex].address, currentFieldValue);
        break;
      case 2:
        strcpy(thermostats[currentThermostatIndex].key, currentFieldValue);
        break;
      }
      memset(currentFieldValue, 0, 50);
      currentFieldIndex++;
      currentFieldPointer = 0;
      if (body[i] == '\n')
      {
        currentFieldIndex = 0;
        thermostats[currentThermostatIndex].lastRead = 0;
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
    if (thermostats[i].lastRead > 0 && (thermostats[i].lastRead + 20 * 60 * 1000) > millis())
    {
      active++;
    }
  }
  return active;
}