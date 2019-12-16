#include "Arduino.h"
#include "Sens.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

SimpleSensor::SimpleSensor()
{
}

SimpleSensor::SimpleSensor(int _pin, int _thresholdSet, int _thresholdReset, boolean highSet, int _timeout, char _sensorName[], long _breukTeller)
{
  //get sensor settings
  sensorPin = _pin;
  thresholdSet = _thresholdSet;
  thresholdReset = _thresholdReset;
  timeout = _timeout;
  breukTeller = _breukTeller;
  _highSet = highSet;

  strncpy(sensorName, _sensorName, 31);

  //setup correct first state for handle()
  makeHigh();
  makeHighMicros = micros();

  // init all defaults
  minuteData = 0;
  rawSensorData = 0;
  sensorMin = 9999;
  sensorMax = 0;
  fired = false;

  avgIndex = 0;
  sensorData = 0;

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
  boolean set = _highSet ? sensorData >= thresholdSet : sensorData <= thresholdSet;
  boolean reset = _highSet ? sensorData <= thresholdReset : sensorData >= thresholdReset;

  if (!fired && set)
  {
    fired = true;
    minuteData++;
    statData++;
    digitalWrite(2, !digitalRead(2)); //DEBUG remove maybe

    //Serial.println(sensorName); //DEBUG

    liveDataInterval = millis() - liveDataMillis;
    liveDataMillis = millis();
  }
  else if (fired && reset)
  {
    fired = false;
  }
}

void SimpleSensor::startReading()
{
  if (!digitalRead(sensorPin))
  {
    makeHigh();
    makeHighMicros = micros();
  }

  while (makeHighMicros - micros() < 100)
  {
    delayMicroseconds(1);
  }

  makeInput();
  startReadingMicros = micros();
  readComplete = false;

  rawSensorData = 0;
}

void SimpleSensor::read()
{
  if (!readComplete)
  {
    if (checkInput())
    {
      readComplete = true;
      rawSensorData = micros() - startReadingMicros;
    }
  }
}

void SimpleSensor::endReading()
{
  if (!readComplete)
  {
    rawSensorData = 9999;
  }

  avg[avgIndex] = rawSensorData;
  avgIndex++;
  if (avgIndex >= avgSamples)
  {
    avgIndex = 0;
  }

  sensorData = 0;

  for (int i = 0; i < avgSamples; i++)
  {
    sensorData += avg[i];
  }

  sensorData /= avgSamples;

  if (sensorData > sensorMax)
    sensorMax = sensorData;

  if (sensorData < sensorMin)
    sensorMin = sensorData;

  checkThreshold();
  makeHigh();
  makeHighMicros = micros();
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

  sensorMin = 9999;
  sensorMax = 0;

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

  state = READY;
}

void DoubleSensor::startReading(boolean left)
{
  if (left)
    leftSensor.startReading();
  else
    rightSensor.startReading();
}

void DoubleSensor::read(boolean left)
{
  if (left)
    leftSensor.read();
  else
    rightSensor.read();
}

void DoubleSensor::endReading(boolean left)
{
  if (left)
    leftSensor.endReading();
  else
    rightSensor.endReading();
}

int debug = 0;

void DoubleSensor::handleDouble()
{
  // invert because vermogenSensors are inverted
  boolean leftFired = leftSensor.fired;
  boolean rightFired = rightSensor.fired;

  switch (state)
  {
  case READY:
    if (leftFired)
    {
      state = TRIGGERED;
      direction = LEFT;
      //Serial.printf("TRIGGERED\n"); //DEBUG
    }
    else if (rightFired)
    {
      state = TRIGGERED;
      direction = RIGHT;
      // Serial.printf("TRIGGERED\n"); //DEBUG
    }
    break;
  case TRIGGERED:
    if (leftFired && rightFired)
    {
      state = ARMED;
      //Serial.printf("ARMED\n"); //DEBUG
    }
    else if (!leftFired && !rightFired)
    {
      state = READY;
      // Serial.printf("READY\n"); //DEBUG
    }
    break;
  case ARMED:
    if (!leftFired && rightFired)
    {
      switch (direction)
      {
      case LEFT:
        state = FIRED;
        //Serial.printf("FIRED\n"); //DEBUG
        break;
      case RIGHT:
        state = TRIGGERED;
        // Serial.printf("TRIGGERED\n"); //DEBUG
        break;
      }
    }
    else if (leftFired && !rightFired)
    {
      switch (direction)
      {
      case LEFT:
        state = TRIGGERED;
        //Serial.printf("TRIGGERED\n"); //DEBUG
        break;
      case RIGHT:
        state = FIRED;
        //Serial.printf("FIRED\n"); //DEBUG
        break;
      }
    }
    break;
  case FIRED:
    if (!leftFired && !rightFired)
    {
      //ACTUAL DONE
      if (direction)
      { //if 1 turn to the right
        minuteData--;

        if (sensorData == 1) //TODO impove this
          sensorData = 0;
        else
          sensorData = -1;

        digitalWrite(2, !digitalRead(2));

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

        digitalWrite(2, !digitalRead(2));

        liveDataInterval = millis() - liveDataMillis;

        liveDataMillis = millis();
      }
      //Serial.printf("%s, %i , %i\n", sensorName, sensorData, debug++); //DEBUG
      //Serial.printf("READY\n");                                        //DEBUG
      state = READY;
    }
    else if (leftFired && rightFired)
    {
      state = ARMED;
      //Serial.printf("ARMED\n"); //DEBUG
    }
    break;
  }
  /*
  if (!leftFired && !rightFired) //Reset just incase
  {
    state = READY;
    Serial.printf("READY"); //DEBUG
  }
  */
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