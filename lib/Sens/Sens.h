#ifndef Sens_h
#define Sens_h

#include "Arduino.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

class SimpleSensor
{
public:
  SimpleSensor();
  SimpleSensor(int _pin, int _thresholdSet, int _thresholdReset, int _timeout, char _sensorName[], long _breukTeller);

  int getSensorData();
  int getLiveData();
  int getMinuteData(boolean reset);

  void makeHigh();
  void makeInput();
  boolean checkInput();
  void checkThreshold();
  boolean handle();

  void addLiveDataToJson(JsonArray arr);
  void addMinuteDataToJson(JsonArray arr);
  void addStatToJson(JsonArray arr);

  unsigned long handleTimestamp;
  unsigned long MAXHANDLETIME = 70; //TEST reduce to minimum
  boolean invalid;
  int invalidCount;

  int sensorPin;
  char sensorName[32];
  int thresholdSet;
  int thresholdReset;
  int timeout;

  int handles;
  int statData;
  long statMillis;

  int sensorData;
  int sensorMin;
  int sensorMax;
  boolean fired;

  int liveData;
  unsigned long liveDataMillis;
  unsigned long liveDataInterval;
  long breukTeller;

  long minuteData;

private:
  enum states
  {
    MAKEINPUT,
    CHECKINPUT
  };

  enum states state;

  long stateTime;
};

class DoubleSensor
{
public:
  DoubleSensor(SimpleSensor _leftSensor, SimpleSensor _rightSensor, char _sensorName[], long _breukTeller);

  int getLiveData();
  int getMinuteData(boolean reset);

  void handle();

  void addLiveDataToJson(JsonArray arr);
  void addMinuteDataToJson(JsonArray arr);
  void addStatToJson(JsonArray arr);

  SimpleSensor leftSensor;
  SimpleSensor rightSensor;

  char sensorName[32];

  int sensorData;

  int liveData;
  unsigned long liveDataMillis;
  unsigned long liveDataInterval;
  long breukTeller;

  long minuteData;

private:
  enum directions
  {
    LEFT,
    RIGHT
  };

  enum directions direction;

  enum states
  {
    READY,
    TRIGGERED,
    ARMED,
    FIRED,
  };

  enum states state;
};

#endif
