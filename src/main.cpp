//TODO add OTA uploading
//TODO add OTA mqtt setting changes (thresholds)
//TODO add last will and status message /home/meterkast/status

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
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

//wifi credentials
const char *ssid = STASSID;
const char *password = STAPSK;

const char *OTAauth = OTAAUTH;

//mqtt credentials
const char *mqttServer = "192.168.0.100";
const int mqttPort = 1883;

//status topic
const char statusTopic[] = "home/meterkast/status";
const char ipTopic[] = "home/meterkast/ip";

WiFiClient espClient;               //wifi client for mqtt
PubSubClient MQTTclient(espClient); // mqtt client
unsigned long MqttReconnectAttempt = 0;

WiFiEventHandler mDisConnectHandler;
WiFiEventHandler mConnectHandler;

int rcTickLimit = 3000; //maximum ticks for the ReadSensor() function

SimpleSensor sensors[] =
    {
        //input sensors
        //SimpleSensor(16, 1000, 2000, (char *)"zon1", 3600000), //zon1
        //SimpleSensor(14, 1060, 1480, (char *)"water", 3600000) //water
        SimpleSensor(12, 1000, 2000, (char *)"zon1", 3600000), //zon1
        SimpleSensor(13, 1060, 1480, (char *)"water", 3600000) //water
                                                               //
};

unsigned long currentMillis = 0; // used for timing
unsigned long minuteTimestamp = 0;
unsigned long liveTimestamp = 0;

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
  minuteTimestamp = currentMillis;
  liveTimestamp = currentMillis;
}
int i = 0;
// the loop function runs over and over again forever
void loop()
{
  currentMillis = millis();

  if (!MQTTclient.connected())
  {
    ConnectToMQTT();
  }

  if (currentMillis - minuteTimestamp > 60 * 1000)
  {                               // Do every minute
    minuteTimestamp += 60 * 1000; // add one minute to current timestamp

    for (auto &sensor : sensors)
    { // for all sensors publish minute data
      //Serial.println(sensor.minuteData);
      sensor.publishMinuteData(MQTTclient);
    }
  }

  if (currentMillis - liveTimestamp > 1 * 1000)
  {                            // Do every Second
    liveTimestamp += 1 * 1000; // add one second to current timestamp

    for (auto &sensor : sensors)
    { // for all sensors publish live data
      sensor.publishLiveData(MQTTclient);
    }

    Serial.println(i);
    i = 0;
  }

  ReadSensors();
  i++;

  /*
  Serial.print("0,1000,2000,3000,");
  Serial.print(sensors[0].sensorData);
  Serial.print(",");
  Serial.println(sensors[1].sensorData);
  //Serial.println(sensors[0].sensorData);
*/
  ArduinoOTA.handle();
  MQTTclient.loop();
}

void ReadSensors()
{
  for (auto &sensor : sensors)
  {
    sensor.makeHigh();
  }

  delayMicroseconds(100); // wait a  ms to make sure cap is discharged

  for (auto &sensor : sensors)
  {
    sensor.makeInput();
  }

  for (int ticks = 0; ticks <= rcTickLimit; ticks++)
  {
    for (auto &sensor : sensors)
    {
      sensor.checkInput(ticks);
    }
  }

  for (auto &sensor : sensors)
  {
    sensor.checkThreshold();
  }

  return;
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
    if (MQTTclient.connect("ESPmeterkast", statusTopic, 1, true, "offline"))
    {
      //MQTTclient.publish("outTopic", "hello world"); //todo connection topic / lastwill
      MQTTclient.publish(statusTopic, "online", true);
      MQTTclient.publish(ipTopic, WiFi.localIP().toString().c_str(), true);

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