//TODO add OTA uploading
//TODO add OTA mqtt setting changes (thresholds)
//TODO add last will and status message /home/meterkast/status

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <Sens.h>
#include <ArduinoJson.h>
#include "credentials.h"

// Function prototypes
void ReadSensors();
void ConnectToWifi();
void ConnectToMQTT();
void MQTTcallback(char *topic, byte *payload, unsigned int length);
void onDisconnect(const WiFiEventStationModeDisconnected &event);
void onConnect(const WiFiEventStationModeConnected &event);
void startOTA();
void PublishLiveData(unsigned long interval);
void PublishMinuteData(unsigned long interval);
void PublishStat(unsigned long interval);

//wifi credentials
const char *ssid = STASSID;
const char *password = STAPSK;

const char *OTAauth = OTAAUTH;

//mqtt credentials
const char *mqttServer = "192.168.0.100";
const int mqttPort = 1883;

//status topic
const char statusTopic[] = "home/meterkast/status";
const char statTopic[] = "home/meterkast/stat";
const char liveTopic[] = "home/meterkast/livedata";
const char minuteTopic[] = "home/meterkast/minutedata";

WiFiClient espClient;               //wifi client for mqtt
PubSubClient MQTTclient(espClient); // mqtt client
unsigned long MqttReconnectAttempt = 0;

WiFiEventHandler mDisConnectHandler;
WiFiEventHandler mConnectHandler;

//TODO reduce to minimum
int rcTickLimit = 3000; //maximum ticks for the ReadSensor() function

SimpleSensor zonSensor = SimpleSensor(14, 1000, 2000, 3000, (char *)"zon1", 3600000);
SimpleSensor waterSensor = SimpleSensor(16, 1060, 1480, 3000, (char *)"water", 3600000);

DoubleSensor vermogenSensor = DoubleSensor(SimpleSensor(12, 1000, 2000, 3000, "vermogenLeft", 1), SimpleSensor(13, 1000, 2000, 3000, "vermogenRight", 1), "vermogen", 3600000);

// used for timing
unsigned long currentMillis = 0;
unsigned long minuteTimestamp = 10000;
unsigned long liveTimestamp = 10000;
unsigned long statusTimestamp = 10000;

// the setup function runs once when you press reset or power the board
void setup()
{
  Serial.begin(500000);

  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  Serial.println();
  Serial.println("Booting...");

  mDisConnectHandler = WiFi.onStationModeDisconnected(onDisconnect);
  mConnectHandler = WiFi.onStationModeConnected(onConnect);

  MQTTclient.setServer(mqttServer, mqttPort);
  MQTTclient.setCallback(MQTTcallback);

  ConnectToWifi();
  ConnectToMQTT();

  startOTA();

  currentMillis = millis();
  //minuteTimestamp = currentMillis;
  //liveTimestamp = currentMillis;
}

// the loop function runs over and over again forever
void loop()
{
  currentMillis = millis();

  if (!MQTTclient.connected())
  {
    ConnectToMQTT();
  }

  PublishLiveData((unsigned long)1 * 1000);
  PublishMinuteData((unsigned long)10 * 1000);
  PublishStat((unsigned long)10 * 1000);

  zonSensor.handle();
  waterSensor.handle();
  vermogenSensor.handle();

  ArduinoOTA.handle();
  MQTTclient.loop();
}

void PublishLiveData(unsigned long interval)
{

  if (currentMillis - liveTimestamp > interval)
  {                            // Do every Second
    liveTimestamp += interval; // add one second to current timestamp

    StaticJsonDocument<256> doc;
    char buffer[256];

    doc["type"] = "liveData";
    doc["time"] = millis();

    JsonArray sensors = doc.createNestedArray("sensors");

    vermogenSensor.addLiveDataToJson(sensors);
    waterSensor.addLiveDataToJson(sensors);
    zonSensor.addLiveDataToJson(sensors);

    size_t n = serializeJson(doc, buffer);

    MQTTclient.beginPublish(liveTopic, n, false);
    for (size_t i = 0; i < n; i++)
    {
      MQTTclient.write(buffer[i]);
    }
    MQTTclient.endPublish();

    //Serial.println(buffer);
  }
}

void PublishMinuteData(unsigned long interval) //TODO finsih
{
  //TODO add timestamp RTC
  //TODO if failed store data locally and push later
  if (currentMillis - minuteTimestamp > interval)
  {                              // Do every minute
    minuteTimestamp += interval; // add one minute to current timestamp

    StaticJsonDocument<512> doc;
    char buffer[512];

    doc["type"] = "minuteData";
    doc["time"] = millis();

    JsonArray sensors = doc.createNestedArray("sensors");

    vermogenSensor.addMinuteDataToJson(sensors);
    waterSensor.addMinuteDataToJson(sensors);
    zonSensor.addMinuteDataToJson(sensors);

    size_t n = serializeJson(doc, buffer);

    MQTTclient.beginPublish(minuteTopic, n, false);
    for (size_t i = 0; i < n; i++)
    {
      MQTTclient.write(buffer[i]);
    }
    MQTTclient.endPublish();

    //Serial.println(buffer);
  }
}

void PublishStat(unsigned long interval) //TODO finsih
{
  if (currentMillis - statusTimestamp > interval) // if interval has passed prepare and send the data
  {
    statusTimestamp += interval; //add interval to timestamp

    StaticJsonDocument<512> doc;
    char buffer[512];

    doc["type"] = "stat";
    doc["time"] = millis();

    JsonArray sensors = doc.createNestedArray("sensors");

    vermogenSensor.addStatusToJson(sensors);
    waterSensor.addStatusToJson(sensors);
    zonSensor.addStatusToJson(sensors);

    size_t n = serializeJson(doc, buffer);

    MQTTclient.beginPublish(statTopic, n, false);
    for (size_t i = 0; i < n; i++)
    {
      MQTTclient.write(buffer[i]);
    }
    MQTTclient.endPublish();

    //Serial.println(buffer);
  }
}

void ConnectToWifi()
{

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500); //todo rewrite without delay
    Serial.print(".");
  }
  //Serial.println("..Connected!");
  Serial.print("Connected to Wifi - ip: ");
  Serial.println(WiFi.localIP());
}

void ConnectToMQTT()
{
  if (currentMillis - MqttReconnectAttempt > 1000)
  {
    MqttReconnectAttempt = currentMillis;
    if (MQTTclient.connect("ESPmeterkast", statusTopic, 1, true, "{\"status\":\"offline\"}"))
    {
      //MQTTclient.publish("outTopic", "hello world"); //todo connection topic / lastwill

      StaticJsonDocument<64> doc;
      char buffer[512];

      doc["time"] = millis();
      doc["status"] = "online";
      doc["ip"] = WiFi.localIP().toString();

      serializeJson(doc, buffer);

      MQTTclient.publish(statusTopic, buffer, true);

      Serial.println("Connected to MQTT");

      MqttReconnectAttempt = 0;
    }
  }
}

void MQTTcallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
}

void onDisconnect(const WiFiEventStationModeDisconnected &event)
{
  Serial.println("Disconnected From Wifi attempting reconnect..."); //todo improve reconnection if failed
  WiFi.reconnect();
}

void onConnect(const WiFiEventStationModeConnected &event)
{
}

void startOTA()
{
  ArduinoOTA.setPasswordHash(OTAauth);

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}