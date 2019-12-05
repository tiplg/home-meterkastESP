#include "Arduino.h"
#include "Sens.h"
#include <PubSubClient.h>

SimpleSensor::SimpleSensor(int _pin, int _thresholdSet, int _thresholdReset, int _timeout, char _sensorName[], long _breukTeller)
{
  //get sensor settings
  sensorPin = _pin;
  thresholdSet = _thresholdSet;
  thresholdReset = _thresholdReset;
  timeout = _timeout;
  breukTeller = _breukTeller;

  //generate minute topic
  strncpy(minuteTopic, "home/", 31);
  strncat(minuteTopic, _sensorName, 31);
  strncat(minuteTopic, "/minuteData", 31);

  //generate live topic
  strncpy(liveTopic, "home/", 31);
  strncat(liveTopic, _sensorName, 31);
  strncat(liveTopic, "/liveData", 31);

  //setup correct first state for handle()
  makeHigh();
  stateTime = micros();
  state = MAKEINPUT;

  // init all defaults
  minuteData = 0;
  sensorData = 0;
  sensorMin = 9999;
  sensorMax = 0;
  armed = true;

  //set current time
  liveTimestamp = millis();
}

int SimpleSensor::getSensorData()
{
  return sensorData;
}

void SimpleSensor::makeHigh()
{
  pinMode(sensorPin, OUTPUT);    // make pin OUTPUT
  digitalWrite(sensorPin, HIGH); // make pin HIGH to discharge capacitor - study the schematic
}

void SimpleSensor::makeInput()
{
  pinMode(sensorPin, INPUT);    // turn pin into an input and time till pin goes low
  digitalWrite(sensorPin, LOW); // turn pullups off - or it won't work
}

boolean SimpleSensor::checkInput()
{
  return !digitalRead(sensorPin);
}

void SimpleSensor::checkThreshold()
{
  if (armed && sensorData < thresholdSet)
  {
    armed = false;
    minuteData++;
    digitalWrite(2, !digitalRead(2)); //TODO remove maybe

    interval = millis() - liveTimestamp;
    liveData = breukTeller / interval;
    liveTimestamp = millis();

    //Serial.println(liveData);
  }
  else if (!armed && sensorData > thresholdReset)
  {
    armed = true;
  }

  if (sensorData > sensorMax)
    sensorMax = sensorData;

  if (sensorData < sensorMin)
    sensorMin = sensorData;
}

void SimpleSensor::handle()
{
  long timeElapsed = micros() - stateTime;
  switch (state)
  {
  case MAKEINPUT:
    if (timeElapsed > 100)
    {
      makeInput();
      stateTime = micros();
      state = CHECKINPUT;
    }
    break;
  case CHECKINPUT:
  {
    long timeElapsed = micros() - stateTime;
    if (timeElapsed > timeout || checkInput())
    {
      samples++; //DEBUG

      sensorData = (timeElapsed > timeout) ? timeout : timeElapsed;
      checkThreshold();
      makeHigh();
      stateTime = micros();
      state = MAKEINPUT;
    }
  }
  default:
    break;
  }
}

void SimpleSensor::publishMinuteData(PubSubClient MQTTclient)
{
  //FIXME remove prints
  Serial.print("minuteData: ");
  Serial.print(minuteData);
  Serial.print("   sensorMax: ");
  Serial.print(sensorMax);
  Serial.print("   sensorMin: ");
  Serial.println(sensorMin);

  //TODO add timestamp RTC
  //TODO if failed store data locally and push later
  MQTTclient.publish(minuteTopic, String(minuteData).c_str());

  //TODO send min/max sensor values MQTT
  minuteData = 0;
  sensorMin = 9999;
  sensorMax = 0;
}

void SimpleSensor::publishLiveData(PubSubClient MQTTclient)
{ // TODO improve for long intervals (minimum interval)

  unsigned long newInterval = millis() - liveTimestamp;

  if (newInterval > interval)
  {
    if (newInterval - interval > 3000)
    { // if interval is more then 3 seconds 'late' set 0
      liveData = 0;
    }
    else
    {
      liveData = breukTeller / newInterval;
    }
  }

  MQTTclient.publish(liveTopic, String(liveData).c_str());
}
