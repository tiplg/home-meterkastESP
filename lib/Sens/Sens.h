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

  void makeHigh();
  void makeInput();
  boolean checkInput();
  void checkThreshold();
  void addLiveDataToJson(JsonArray arr);
  void addMinuteDataToJson(JsonArray arr);
  void addStatusToJson(JsonArray arr);
  int getLiveData();
  boolean handle();

  unsigned long handleTimestamp;
  unsigned long MAXHANDLETIME = 200;
  boolean invalid;

  int sensorPin;
  char sensorName[32];
  int thresholdSet;
  int thresholdReset;
  int timeout;

  int samples;
  int statusData;

  int sensorData;
  int sensorMin;
  int sensorMax;
  boolean fired;

  int liveData;
  unsigned long liveTimestamp;
  unsigned long interval;
  long breukTeller;
  char liveTopic[32];

  long minuteData;
  char minuteTopic[32];

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

  void handle();
  int getLiveData();
  void addLiveDataToJson(JsonArray arr);
  void addMinuteDataToJson(JsonArray arr);
  void addStatusToJson(JsonArray arr);

  SimpleSensor leftSensor;
  SimpleSensor rightSensor;

  char sensorName[32];

  int sensorData;

  int liveData;
  unsigned long liveTimestamp;
  unsigned long interval;
  long breukTeller;
  char liveTopic[32];

  long minuteData;
  char minuteTopic[32];

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
