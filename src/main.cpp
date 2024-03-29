//TODO add mqtt setting changes (thresholds) and activate OTA
//TODO runti/me for fun

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <string.h>

#include <Sens.h>
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

//mqtt credentials
const char *mqttId = UUID;
const char *mqttServer = "192.168.0.100";
const int mqttPort = 1883;

//status topic
const char statusTopic[] = "home/meterkast/status";
const char settingTopic[] = "home/meterkast/setting";
const char liveTopic[] = "home/meterkast/livedata";
const char minuteTopic[] = "home/meterkast/minutedata";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 1 * 60 * 60, 60 * 60 * 1000);

WiFiClient espClient;               //wifi client for mqtt
PubSubClient MQTTclient(espClient); // mqtt client
unsigned long MqttReconnectAttempt = 0;

WiFiEventHandler mDisConnectHandler;
WiFiEventHandler mConnectHandler;

boolean otaEnabled = true;
unsigned long otaMillis;
unsigned long otaTimeout = 10 * 1000;

//TEST reduce to minimum
int rcTickLimit = 3000; //maximum ticks for the ReadSensor() function

SimpleSensor zonSensor = SimpleSensor(14, 1800, 1900, false, 2000, 1, (char *)"zon1", 3600000);
SimpleSensor waterSensor = SimpleSensor(16, 1060, 1480, false, 3000, 10, (char *)"water", 3600000);

DoubleSensor vermogenSensor = DoubleSensor(SimpleSensor(12, 807 + 35, 807 + 0, true, 1000, 10, (char *)"vermogenLeft", 1), SimpleSensor(13, 679 + 35., 679 + 0, true, 1000, 10, (char *)"vermogenRight", 1), (char *)"vermogen", 6000000);
//12,13
// used for timing
unsigned long currentMillis = 0;
unsigned long minuteTimestamp = 0;
unsigned long liveTimestamp = 0;
unsigned long statTimestamp = 0;
unsigned long loopSpeedMicros = 0;

long loopCount = 0;

// the setup function runs once when you press reset or power the board
void setup()
{
  Serial.begin(500000);

  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  Serial.println();
  Serial.println("Booting...");
  Serial.println(UUID);

  mDisConnectHandler = WiFi.onStationModeDisconnected(onDisconnect);
  mConnectHandler = WiFi.onStationModeConnected(onConnect);

  MQTTclient.setServer(mqttServer, mqttPort);
  MQTTclient.setCallback(MQTTcallback);

  ConnectToWifi();

  timeClient.begin();
  timeClient.forceUpdate();

  ConnectToMQTT();

  startOTA();

  currentMillis = millis();
  minuteTimestamp = currentMillis;
  liveTimestamp = currentMillis;
  statTimestamp = currentMillis;
  otaMillis = currentMillis;

  loopSpeedMicros = micros();
}

// the loop function runs over and over again forever
void loop()
{
  currentMillis = millis();
  timeClient.update();

  if (!MQTTclient.connected()) //TODO only reconnect when needed
  {
    ConnectToMQTT();
  }

  PublishLiveData(1 * 1000UL);
  PublishMinuteData(60 * 1000UL);
  PublishStat(10 * 60 * 1000UL);

  ReadSensors();

  MQTTclient.loop();

  if (otaEnabled)
  {
    ArduinoOTA.handle();

    if (millis() - otaMillis > otaTimeout)
    {
      otaEnabled = false;

      Serial.println("ota disabled");

      StaticJsonDocument<512> doc;
      char buffer[512];

      doc["type"] = "ota";
      doc["ready"] = false;

      serializeJson(doc, buffer);

      MQTTclient.publish(settingTopic, buffer);
    }
  }
}

void ReadSensors()
{
  for (int i = 0; i < 30; i++)
  {
    vermogenSensor.leftSensor.startReading();
    for (int j = 0; j < vermogenSensor.leftSensor.timeout; j++)
    {
      vermogenSensor.leftSensor.read();
    }
    vermogenSensor.leftSensor.endReading();

    vermogenSensor.rightSensor.startReading();
    for (int j = 0; j < vermogenSensor.rightSensor.timeout; j++)
    {
      vermogenSensor.rightSensor.read();
    }
    vermogenSensor.rightSensor.endReading();

    vermogenSensor.handleDouble();

    if (i % 10 == 0)
    {
      zonSensor.startReading();
      for (int i = 0; i < zonSensor.timeout; i++)
      {
        zonSensor.read();
      }
      zonSensor.endReading();
    }

    //Serial.printf("%i,%i,-40,35,80,%i,%i\n", vermogenSensor.leftSensor.sensorData - 807, vermogenSensor.rightSensor.sensorData - 679, vermogenSensor.leftSensor.fired ? 75 : 10, vermogenSensor.rightSensor.fired ? 76 : 11); //DEBUG
    Serial.printf("%i,0,1000,2000,3000\n", zonSensor.sensorData);
    loopCount++;
  }
}

void PublishLiveData(unsigned long interval)
{

  if (currentMillis - liveTimestamp > interval)
  {                            // Do every Second
    liveTimestamp += interval; // add one second to current timestamp

    StaticJsonDocument<512> doc;
    char buffer[512];

    doc["type"] = "liveData";
    doc["time"] = timeClient.getEpochTime();

    JsonArray sensors = doc.createNestedArray("sensors");

    vermogenSensor.addLiveDataToJson(sensors);
    //waterSensor.addLiveDataToJson(sensors);
    zonSensor.addLiveDataToJson(sensors);

    serializeJson(doc, buffer);

    MQTTclient.publish(liveTopic, buffer);

    //Serial.println(buffer);
  }
}

void PublishMinuteData(unsigned long interval)
{
  //TODO if failed store data locally and push later
  if (currentMillis - minuteTimestamp > interval)
  {                              // Do every minute
    minuteTimestamp += interval; // add one minute to current timestamp

    StaticJsonDocument<512> doc;
    char buffer[512];

    doc["type"] = "minuteData";
    doc["time"] = timeClient.getEpochTime();

    JsonArray sensors = doc.createNestedArray("sensors");

    vermogenSensor.addMinuteDataToJson(sensors);
    //waterSensor.addMinuteDataToJson(sensors);
    zonSensor.addMinuteDataToJson(sensors);

    serializeJson(doc, buffer);

    MQTTclient.publish(minuteTopic, buffer);

    //Serial.println(buffer);
  }
}

void PublishStat(unsigned long interval)
{
  if (currentMillis - statTimestamp > interval) // if interval has passed prepare and send the data
  {
    statTimestamp += interval; //add interval to timestamp

    StaticJsonDocument<512> doc;
    char buffer[512];

    doc["type"] = "stat";
    doc["time"] = timeClient.getEpochTime();
    doc["loopSpeed"] = (micros() - loopSpeedMicros) / loopCount;

    loopCount = 0;
    loopSpeedMicros = micros();

    JsonArray sensors = doc.createNestedArray("sensors");

    vermogenSensor.addStatToJson(sensors);
    //waterSensor.addStatToJson(sensors);
    zonSensor.addStatToJson(sensors);

    serializeJson(doc, buffer);

    MQTTclient.publish(settingTopic, buffer);

    //Serial.println(buffer); //DEBUG
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

  Serial.print("Connected to Wifi - ip: ");
  Serial.println(WiFi.localIP());
}

void ConnectToMQTT()
{
  if (currentMillis - MqttReconnectAttempt > 1000)
  {
    MqttReconnectAttempt = currentMillis;
    if (MQTTclient.connect(mqttId, statusTopic, 1, true, "{\"status\":\"offline\"}"))
    {
      StaticJsonDocument<512> doc;
      char buffer[512];

      doc["time"] = timeClient.getEpochTime();
      doc["status"] = "online";
      doc["id"] = mqttId;
      doc["ip"] = WiFi.localIP().toString();

      serializeJson(doc, buffer);

      MQTTclient.publish(statusTopic, buffer, true);

      MQTTclient.subscribe(settingTopic);

      Serial.println("Connected to MQTT");

      MqttReconnectAttempt = 0;
    }
  }
}

void MQTTcallback(char *topic, byte *payload, unsigned int length)
{
  //Serial.print("MQTT:");
  //Serial.println(topic);
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err)
  {
    Serial.print("deserializeJson() failed with code ");
    Serial.println(err.c_str());
  }
  else
  {
    if (doc["type"] == "ota")
    {
      Serial.println("ota enabled");

      char buffer[512];

      doc.clear;
      doc["type"] = "ota";
      doc["ready"] = true;
      doc["timeout"] = otaTimeout;

      serializeJson(doc, buffer);

      MQTTclient.publish(settingTopic, buffer);

      otaEnabled = true;
      otaMillis = currentMillis;
    }
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
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}