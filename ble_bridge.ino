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
#include "mqtt_gateway.h"

#define BYTE_ORDER 1234

int THERMOSTAT_STATUS_UPDATE_INTERVAL = 10 * 60 * 1000;
int GATEWAY_CHECK_INTERVAL = 60 * 1000;
// int MEMORY_CHECK_INTERVAL = 30 * 1000;
int PING_INTERVAL = 30 * 1000;
int BLINK_INTERVAL = 700;

long lastGatewayCheck = 0;
long lastThermostatStatusCheck = 0;
// long lastMemoryCheck = 0;
long lastBlink = 0;
long lastPing = 0;

Thermostat thermostats[10];
int numDevices = 0;

char *errors[100];

char deviceID[30];

int dontUpdate = 1;

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

  while (!taskGetFromGateway())
    ;

  while (!taskReadThermostats())
    ;

  taskSendStatusToGateway();
  resetTimers();
}

void resetTimers()
{
  lastThermostatStatusCheck = lastBlink = lastGatewayCheck = lastPing = millis();
}

void loop()
{
  long now = millis();
  if (now - lastBlink > BLINK_INTERVAL)
  {
    lastBlink = now;
    digitalWrite(LED_BUILTIN, HIGH);
    delay(75);
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (now - lastPing > PING_INTERVAL)
  {
    lastPing = now;
    sendMetrics();
  }

  /*
    if (now - lastMemoryCheck > MEMORY_CHECK_INTERVAL)
    {
      lastMemoryCheck = now;
      Serial.print(F("Memory: "));
      Serial.println(freeMemory());
    }
  */

  if (now - lastGatewayCheck > GATEWAY_CHECK_INTERVAL)
  {
    lastGatewayCheck = now;
    int previousNumDevices = numDevices;
    taskGetFromGateway();
    if (numDevices != previousNumDevices)
    {
      Serial.println("Number of registered devices has changed. Running thermostat update.");
      taskReadThermostats();
    }
  }

  if (now - lastThermostatStatusCheck > THERMOSTAT_STATUS_UPDATE_INTERVAL)
  {
    lastThermostatStatusCheck = now;
    if (taskReadThermostats())
    {
      taskSendStatusToGateway();
    }
  }

  int shouldBeUpdated = thermostatNeedsUpdating();
  if (shouldBeUpdated > -1 && !dontUpdate)
  {
    char updateMessage[100];
    sprintf(updateMessage,
            "Thermostat %s has %f should have %f",
            thermostats[shouldBeUpdated].address,
            thermostats[shouldBeUpdated].temps.set,
            thermostats[shouldBeUpdated].temps.desired);
    Serial.println(updateMessage);
    BLE.begin();
    updateThermostat(shouldBeUpdated);
    readThermostat(shouldBeUpdated);
    BLE.end();
  }
}

int taskGetFromGateway()
{
  Serial.println(F("--- Performing gateway fetch task..."));
  bool connected = connectWiFi();
  if (!connected)
  {
    Serial.println(F("--- Could not connect to Wifi. Skipping gateway check."));
    return 0;
  }
  digitalWrite(LED_BUILTIN, HIGH);

  bool status = gwGetThermostats(messageGateway, messageGatewayPort, deviceID, thermostats, &numDevices);
  if (!status)
  {
    Serial.println(F("--- Could not get thermostats!"));
    return 0;
  }

  WiFi.disconnect();
  WiFi.end();
  Serial.println(F("WiFi disconnected."));
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println(F("--- done"));
  Serial.println();
  return 1;
}

int taskReadThermostats()
{
  Serial.println(F("--- Performing status check task"));
  if (numDevices == 0)
  {
    Serial.println(F("--- No thermostats registered. Skipping."));
    return 0;
  }
  BLE.begin();
  for (int i = 0; i < numDevices; i++)
  {
    if (!readThermostat(i))
    {
      Serial.println(F("Could not read themostat"));
    }
  }
  BLE.end();
  Serial.println(F("--- done"));
  Serial.println();
  return 1;
}

int readThermostat(int index)
{
  BLEDevice thermostatBLE = connect(thermostats[index].address, 30);
  if (!thermostatBLE || !thermostatBLE.connected())
  {
    Serial.print(F("Could not connect to "));
    Serial.println(thermostats[index].address);
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
  return 1;
}

int updateThermostat(int index)
{
  int status = 0;
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
    return 0;
  }

  thermostatBLE.disconnect();
  Serial.println(F("--- done"));
  Serial.println();
  return 1;
}

void taskSendStatusToGateway()
{
  Serial.println(F("--- Starting status write"));
  if (!connectWiFi())
  {
    Serial.println(F("--- Could not connect to WiFi. Skipping status write."));
    return;
  }
  digitalWrite(LED_BUILTIN, HIGH);

  if (!gwSendStatus(messageGateway, messageGatewayPort, thermostats, numDevices))
  {
    Serial.println("Could not send status update.");
  }

  WiFi.disconnect();
  WiFi.end();
  Serial.println(F("WiFi disconnected."));
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println(F("--- done"));
  Serial.println();
}

void sendMetrics()
{
  Serial.println(F("--- Performing metrics task..."));

  if (!connectWiFi())
  {
    Serial.println(F("--- Could not connect to WiFi. Skipping metrics push."));
    return;
  }
  /*
    if (client.connect("192.168.1.52", 3001))
    {
      Serial.println("Pinging");
      // Make a HTTP request:
      client.print("GET /api/push/dtJ2ZXazQu?status=up&msg=OK&ping= HTTP/1.1\r\n");
      client.print("Host: status.spikyhouse.com\r\n");
      client.print("Connection: close\r\n");
      client.print("\r\n");
      client.flush();
      client.stop();
    }
  */
  WiFiClient client;
  if (client.connect("192.168.1.51", 9091))
  {
    // Make a HTTP request:
    char line1[60];
    sprintf(line1, "POST /metrics/job/%s HTTP/1.1\r\n", deviceID);
    client.print(line1);
    client.print("Connection: close\r\n");
    client.print("Content-Type: application/x-www-form-urlencoded\r\n");
    client.print("Content-Length: 24\r\n");
    client.print("Host: 192.168.1.51:9091\r\n");
    client.print("\r\n");
    client.print("memory_free_bytes ");
    client.print(freeMemory(), 10);
    client.print("\n");
    client.flush();
    client.stop();
  }

  delay(200);
  Serial.println(F("Ending WiFi"));
  WiFi.disconnect();
  WiFi.end();
  Serial.println(F("--- done"));
  Serial.println();
}

bool connectWiFi()
{
  wiFiDrv.wifiDriverDeinit();
  wiFiDrv.wifiDriverInit();
  long start = millis();
  int status = WiFi.status();
  while (status != WL_CONNECTED && (millis() - start < 30000))
  {
    Serial.println(F("Attempting WiFi connect..."));
    status = WiFi.begin(ssid, pass);
    delay(500);
    if (status == WL_CONNECTED)
    {
      Serial.println(F("WiFI connected!"));
      return true;
    }
  }
  return false;
}

int thermostatNeedsUpdating()
{
  for (int i = 0; i < numDevices; i++)
  {
    if (thermostats[i].temps.desired != thermostats[i].temps.set)
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