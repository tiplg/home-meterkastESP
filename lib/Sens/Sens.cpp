#include "Arduino.h"
#include "Sens.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

SimpleSensor::SimpleSensor()
{
}

SimpleSensor::SimpleSensor(int _pin, int _thresholdSet, int _thresholdReset, int _timeout, char _sensorName[], long _breukTeller)
{
  //get sensor settings
  sensorPin = _pin;
  thresholdSet = _thresholdSet;
  thresholdReset = _thresholdReset;
  timeout = _timeout;
  breukTeller = _breukTeller;

  strncpy(sensorName, _sensorName, 31);

  //setup correct first state for handle()
  makeHigh();
  stateTime = micros();
  state = MAKEINPUT;

  // init all defaults
  minuteData = 0;
  sensorData = 0;
  sensorMin = 9999;
  sensorMax = 0;
  handles = 0;
  invalidCount = 0;
  fired = false;

  //set current time
  liveDataMillis = millis();
  statMillis = liveDataMillis;
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
  if (!fired && sensorData < thresholdSet)
  {
    fired = true;
    minuteData++;
    statData++;
    digitalWrite(2, !digitalRead(2)); //DEBUG remove maybe

    liveDataInterval = millis() - liveDataMillis;
    liveDataMillis = millis();
  }
  else if (fired && sensorData > thresholdReset)
  {
    fired = false;
  }

  if (sensorData > sensorMax)
    sensorMax = sensorData;

  if (sensorData < sensorMin)
    sensorMin = sensorData;
}

boolean SimpleSensor::handle()
{
  long timeElapsed = micros() - stateTime; // this time interval is used for the states
  handles++;

  if (micros() - handleTimestamp > MAXHANDLETIME) // if handle time is to long sensorData might not be acurate
  {
    invalid = true;
    invalidCount++;
  }
  else
  {
    invalid = false;
  }
  handleTimestamp = micros();

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
      if (!invalid) // if not invalid sensorData can be used to check thrersholds
      {
        sensorData = (timeElapsed > timeout) ? timeout : timeElapsed;
        checkThreshold();
      }

      makeHigh();
      stateTime = micros();
      state = MAKEINPUT;
    }
  }
  }

  return !invalid;
}

int SimpleSensor::getLiveData()
{
  unsigned long newInterval = millis() - liveDataMillis;

  if (newInterval > liveDataInterval)
  {
    if (newInterval - liveDataInterval > 3000)
    { // if interval is more then 3 seconds 'late' set 0
      liveData = 0;
    }
    else
    {
      liveData = breukTeller / newInterval;
    }
  }
  else
  {
    liveData = breukTeller / liveDataInterval;
  }

  return liveData;
}

int SimpleSensor::getMinuteData(boolean reset)
{
  int result = minuteData;

  if (reset)
  {
    minuteData = 0;
  }

  return result;
}

void SimpleSensor::addLiveDataToJson(JsonArray arr)
{
  JsonObject obj = arr.createNestedObject();
  obj["sensorName"] = sensorName;
  obj["liveData"] = getLiveData();
}

void SimpleSensor::addMinuteDataToJson(JsonArray arr)
{
  JsonObject obj = arr.createNestedObject();
  obj["sensorName"] = sensorName;
  obj["minuteData"] = getMinuteData(true);
}

void SimpleSensor::addStatToJson(JsonArray arr)
{
  JsonObject obj = arr.createNestedObject();
  obj["sensorName"] = sensorName;
  obj["sensorMin"] = sensorMin;
  obj["sensorMax"] = sensorMax;
  obj["handleTime"] = (millis() - statMillis) * 1000 / handles;
  obj["invalidCount"] = invalidCount;

  sensorMin = 9999;
  sensorMax = 0;
  handles = 0;
  invalidCount = 0;
  statMillis = millis();
}

DoubleSensor::DoubleSensor(SimpleSensor _leftSensor, SimpleSensor _rightSensor, char _sensorName[], long _breukTeller)
{
  leftSensor = _leftSensor;
  rightSensor = _rightSensor;

  breukTeller = _breukTeller;

  //copy sensor name
  strncpy(sensorName, _sensorName, 31);

  // init all defaults
  sensorData = 0;
  minuteData = 0;

  //set current time
  liveDataMillis = millis();
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
      { //if 1 turn to the right
        minuteData--;

        if (sensorData == 1) //TODO impove this
          sensorData = 0;
        else
          sensorData = -1;

        digitalWrite(2, !digitalRead(2)); //DEBUG remove maybe

        liveDataInterval = millis() - liveDataMillis;

        liveDataMillis = millis();
      }
      else
      { //if 1 turn to the left
        minuteData++;

        if (sensorData == -1)
          sensorData = 0;
        else
          sensorData = 1;

        digitalWrite(2, !digitalRead(2)); //DEBUG remove maybe

        liveDataInterval = millis() - liveDataMillis;

        liveDataMillis = millis();
      }

      state = READY;
    }
    else if (leftSensor.fired && rightSensor.fired)
    {
      state = ARMED;
    }
    break;
  }

  if (!leftSensor.fired && !rightSensor.fired) //Reset just incase
  {
    state = READY;
  }
}

int DoubleSensor::getLiveData()
{
  unsigned long newInterval = millis() - liveDataMillis;

  if (newInterval > liveDataInterval)
  {
    if (newInterval - liveDataInterval > 3000)
    { // if interval is more then 3 seconds 'late' set 0
      liveData = 0;
    }
    else
    {
      liveData = breukTeller / newInterval * sensorData;
    }
  }
  else
  {
    liveData = breukTeller / liveDataInterval * sensorData;
  }

  return liveData;
}

int DoubleSensor::getMinuteData(boolean reset)
{
  int result = minuteData;

  if (reset)
  {
    minuteData = 0;
  }

  return result;
}

void DoubleSensor::addLiveDataToJson(JsonArray arr)
{
  JsonObject obj = arr.createNestedObject();
  obj["sensorName"] = sensorName;
  obj["liveData"] = getLiveData();
}

void DoubleSensor::addMinuteDataToJson(JsonArray arr)
{
  JsonObject obj = arr.createNestedObject();
  obj["sensorName"] = sensorName;
  obj["minuteData"] = getMinuteData(true);
}

void DoubleSensor::addStatToJson(JsonArray arr)
{
  leftSensor.addStatToJson(arr);
  rightSensor.addStatToJson(arr);
}