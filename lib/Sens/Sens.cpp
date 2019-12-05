#include "Arduino.h"
#include "Sens.h"
#include <PubSubClient.h>

SimpleSensor::SimpleSensor()
{
}

SimpleSensor::SimpleSensor(int _pin, int _thresholdSet, int _thresholdReset, int _timeout)
{
  //get sensor settings
  sensorPin = _pin;
  thresholdSet = _thresholdSet;
  thresholdReset = _thresholdReset;
  timeout = _timeout;

  breukTeller = 1000000; // Dummy value

  //setup correct first state for handle()
  makeHigh();
  stateTime = micros();
  state = MAKEINPUT;

  // init all defaults
  minuteData = 0;
  sensorData = 0;
  sensorMin = 9999;
  sensorMax = 0;
  fired = false;

  //set current time
  liveTimestamp = millis();
}

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
  fired = false;

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

boolean SimpleSensor::checkThreshold()
{
  boolean result = false;
  if (!fired && sensorData < thresholdSet)
  {
    result = true;
    fired = true;
    minuteData++;
    digitalWrite(2, !digitalRead(2)); //TODO remove maybe

    interval = millis() - liveTimestamp;
    //liveData = breukTeller / interval;
    liveTimestamp = millis();

    //Serial.println(liveData);
  }
  else if (fired && sensorData > thresholdReset)
  {
    fired = false;
  }

  if (sensorData > sensorMax)
    sensorMax = sensorData;

  if (sensorData < sensorMin)
    sensorMin = sensorData;

  return result;
}

boolean SimpleSensor::handle()
{
  boolean result = false;
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
      result = checkThreshold();
      makeHigh();
      stateTime = micros();
      state = MAKEINPUT;
    }
  }
  }

  return result;
}

void SimpleSensor::publishMinuteData(PubSubClient MQTTclient)
{
  //DEBUG remove prints
  Serial.printf("%s - minuteData: %i sensorMax: %i sensorMin: %i", sensorName, minuteData, sensorMax, sensorMin);

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

DoubleSensor::DoubleSensor(SimpleSensor _leftSensor, SimpleSensor _rightSensor)
{
  leftSensor = _leftSensor;
  rightSensor = _rightSensor;
}

void DoubleSensor::handle() //TODO this should be not that hard ?!
{
  leftSensor.handle();
  rightSensor.handle();

  switch (state)
  {
  case READY:
    if (leftSensor.fired)
    {
      state = TRIGGERED;
      direction = LEFT;
    }
    else if (rightSensor.fired)
    {
      state = TRIGGERED;
      direction = RIGHT;
    }
    break;
  case TRIGGERED:
    if (leftSensor.fired && rightSensor.fired)
    {
      state = ARMED;
    }
    else if (!leftSensor.fired && !rightSensor.fired)
    {
      state = READY;
    }
    break;
  case ARMED:
    if (!leftSensor.fired && rightSensor.fired)
    {
      switch (direction)
      {
      case LEFT:
        state = FIRED;
        break;
      case RIGHT:
        state = TRIGGERED;
        break;
      }
    }
    else if (leftSensor.fired && !rightSensor.fired)
    {
      switch (direction)
      {
      case LEFT:
        state = TRIGGERED;
        break;
      case RIGHT:
        state = FIRED;
        break;
      }
    }
    break;
  case FIRED:
    if (!leftSensor.fired && !rightSensor.fired)
    {
      //ACTUAL DONE
      if (direction)
      {
        Serial.println("FIRED LEFT!");
      }
      else
      {
        Serial.println("FIRED RIGHT!");
      }

      state = READY;
    }
    else if (leftSensor.fired && rightSensor.fired)
    {
      state = ARMED;
    }
    break;
  }
}