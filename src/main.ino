#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Preferences.h>

#include "SHTSensor.h"
#include "config.h"

#define LED 2
#define MAX_MSG_LEN 256
#define MSG_TIMEOUT_MILLIS 100

WiFiClient espClient;
PubSubClient client(espClient);
SHTSensor sht;
Preferences preferences;
DynamicJsonDocument msg(1024);

String deviceName;

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  preferences.begin("humimon", false);

  deviceName = preferences.getString("deviceName", "");
  if (deviceName == "")
  {
    deviceName = DEFAULT_NAME;
    preferences.putString("deviceName", deviceName);
  }
  preferences.end();
  msg["device"] = deviceName;

  if (!sht.init())
  {
    Serial.println("init(): failed\n");
  }
  sht.setAccuracy(SHTSensor::SHT_ACCURACY_HIGH);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.hostname("SENS");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(LED, LOW);
    delay(200);
    digitalWrite(LED, HIGH);
    delay(200);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");

  Serial.println("WiFi connected");
  Serial.print("Device name:");
  Serial.println(deviceName);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());

  client.setServer(MQTT_SERVER, 1883);
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Connecting to MQTT...");
    String clientId = "HUMI-" + WiFi.macAddress();
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(1000);
    }
  }
}

double round2(double value)
{
  return (int)(value * 100 + 0.5) / 100.0;
}

long lastSampleTime = 0;

void loop()
{
  if (!client.connected())
  {
    digitalWrite(LED, LOW);
    reconnect();
    digitalWrite(LED, HIGH);
  }

  digitalWrite(LED, millis() % 1000 < 200 ? LOW : HIGH);
  client.loop();

  if (millis() - lastSampleTime > 1000)
  {
    lastSampleTime = millis();

    if (!sht.readSample())
    {
      Serial.println("Error in readSample()");
      return;
    }

    printf("RH=%.1f%%   TMP=%.2f oC\n", sht.getHumidity(), sht.getTemperature());

    msg["temp"] = round2(sht.getTemperature());
    msg["humidity"] = round2(sht.getHumidity());
    char jsonString[MAX_MSG_LEN];
    serializeJson(msg, jsonString);
    client.publish("HUMIREP", jsonString);
  }
}