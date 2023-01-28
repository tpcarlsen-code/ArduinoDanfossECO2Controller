#include <stdio.h>
#include "mqtt_gateway.h"
#include "WiFiNINA.h"

bool sendRequest(IPAddress gatewayIP, int gatewayPort, const char *method, const char *path, const char *body)
{
    char header[100];
    WiFiClient client;
    if (!client.connect(gatewayIP, gatewayPort))
    {
        Serial.print("Could not connect to gateway!");
        return false;
    }
    sprintf(header, "%s %s HTTP/1.1\r\n", method, path);
    client.print(header);
    client.print("Host: ");
    client.print(gatewayIP);
    client.print(":");
    client.print(gatewayPort);
    client.print("\r\n");
    client.print("Connection: close\r\n");
    if (!strcmp(method, "POST"))
    {
        client.print("Content-Type: text/plain\r\n");
        client.print("Content-Length: ");
        client.print(strlen(body));
        client.print("\r\n");
    }
    client.print("\r\n");
    if (!strcmp(method, "POST"))
    {
        client.print(body);
    }
    client.flush();
    client.stop();
}

bool sendMetric(IPAddress gatewayIP, int gatewayPort, const char *thermostatName, const char *name, const char *value)
{
    char path[50];
    sprintf(path, "/sensor/%s/%s", thermostatName, name);
    sendRequest(gatewayIP, gatewayPort, "POST", path, value);
}

struct httpHeader
{
    int status;
    char message[25];
};

httpHeader readHeader(const uint8_t *response)
{
    httpHeader header;
    int elements = 0;
    int i = 0;
    char u = 0;
    char buf[25];
    int bufC = 0;
    while ((u = (char)response[i++]) != 0)
    {
        if (u == 32 || u == 10 || u == 13)
        {
            buf[bufC] = '\0';
            if (elements == 0)
            {
                bufC = 0;
            }
            if (elements == 1)
            {
                header.status = atoi(buf);
                bufC = 0;
            }
            else if (elements == 2)
            {
                strcpy(header.message, buf);
                return header;
            }
            elements++;
            continue;
        }
        else
        {
            buf[bufC++] = u;
        }
    }
    return header;
}

void readBody(const char *response, char *body)
{
    char *bodyWithExtra;
    // Look for \r\n\r\n which indicates start of body.
    bodyWithExtra = strstr(response, "\r\n\r\n");
    bodyWithExtra += 4;
    strcpy(body, bodyWithExtra);
}

bool gwGetThermostats(IPAddress gatewayIP, int gatewayPort, const char *deviceID, Thermostat *out, int *count)
{
    uint8_t readBuffer[1024];
    WiFiClient client;
    if (!client.connect(gatewayIP, gatewayPort))
    {
        Serial.println("Could not connect to gateway!");
        return false;
    }
    char line1[40];
    sprintf(line1, "GET /thermostats/%s HTTP/1.1\r\n", deviceID);
    client.print(line1);
    client.print("Connection: close\r\n");
    client.print("Host: something.com\r\n");
    client.print("\r\n");
    client.flush();
    int read = 0;
    int c = 0;
    while (client.peek() == -1)
        ; // todo timeout.
    while ((read = client.read()) > -1 && c < 1024)
    {
        readBuffer[c++] = read;
    }
    if (!c)
    {
        Serial.println("Could not read from gateway!");
        return false;
    }
    client.stop();
    readBuffer[c] = 0;

    httpHeader h = readHeader(readBuffer);
    if (h.status != 200)
    {
        Serial.print("Received unexpected status from gateway: ");
        Serial.println(h.status);
        return false;
    }

    char body[512];
    readBody((char *)readBuffer, body);

    if (strlen(body) < 10)
    {
        Serial.print(F("Received unexpected body length from gateway: "));
        Serial.println(strlen(body));
        return false;
    }

    int currentThermostatIndex = 0;
    int currentFieldIndex = 0;
    char currentFieldValue[50];
    int currentFieldPointer = 0;
    float desiredTemp = 0.0;
    for (int i = 0; i < strlen(body); i++)
    {
        if (body[i] == '\n' || body[i] == ' ')
        {
            currentFieldValue[currentFieldPointer] = '\0';
            switch (currentFieldIndex)
            {
            case 0:
                strcpy(out[currentThermostatIndex].friendlyName, currentFieldValue);
                break;
            case 1:
                strcpy(out[currentThermostatIndex].address, currentFieldValue);
                break;
            case 2:
                strcpy(out[currentThermostatIndex].key, currentFieldValue);
                break;
            case 3:
                float t = atof(currentFieldValue);
                out[currentThermostatIndex].temps.desired = t;
                break;
            }
            memset(currentFieldValue, 0, 50);
            currentFieldIndex++;
            if (body[i] == '\n')
            {
                currentFieldIndex = 0;
                currentThermostatIndex++;
            }
            currentFieldPointer = 0;
        }
        else
        {
            currentFieldValue[currentFieldPointer++] = body[i];
        }
    }

    for (int i = 0; i < currentThermostatIndex; i++)
    {
        Serial.print(out[i].friendlyName);
        Serial.print(";");
        Serial.print(out[i].address);
        Serial.print(";");
        Serial.print(out[i].key);
        Serial.print(";");
        Serial.print(out[i].temps.desired);
        Serial.println();
    }
    *count = currentThermostatIndex;

    return true;
}

bool gwSendStatus(IPAddress gatewayIP, int gatewayPort, Thermostat *thermostats, int count)
{
    char value[50];
    for (int i = 0; i < count; i++)
    {
        sprintf(value, "%d", thermostats[i].batteryLevel);
        sendMetric(gatewayIP, gatewayPort, thermostats[i].friendlyName, "battery_level", value);

        sprintf(value, "%d", thermostats[i].rssi);
        sendMetric(gatewayIP, gatewayPort, thermostats[i].friendlyName, "rssi", value);

        sprintf(value, "%f", thermostats[i].temps.measured);
        sendMetric(gatewayIP, gatewayPort, thermostats[i].friendlyName, "room_temperature", value);

        sprintf(value, "%f", thermostats[i].temps.set);
        sendMetric(gatewayIP, gatewayPort, thermostats[i].friendlyName, "set_temperature", value);
    }
}
