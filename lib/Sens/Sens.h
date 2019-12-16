#ifndef Sens_h
#define Sens_h

#include "Arduino.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

class SimpleSensor
{
public:
  SimpleSensor();
  SimpleSensor(int _pin, int _thresholdSet, int _thresholdReset, boolean highSet, int _timeout, char _sensorName[], long _breukTeller);

  int getSensorData();
  int getLiveData();
  int getMinuteData(boolean reset);

  void startReading();
  void read();
  void endReading();

  void makeHigh();
  void makeInput();
  boolean checkInput();
  void checkThreshold();
  boolean handle();

  void addLiveDataToJson(JsonArray arr);
  void addMinuteDataToJson(JsonArray arr);
  void addStatToJson(JsonArray arr);

  unsigned long makeHighMicros;

  unsigned long startReadingMicros;
  boolean readComplete;

  int sensorPin;
  char sensorName[32];
  int thresholdSet;
  int thresholdReset;
  int timeout;
  boolean _highSet;

  int statData;
  long statMillis;

  int rawSensorData;
  int sensorMin;
  int sensorMax;
  boolean fired;

  int sensorData;
  int avg[100];
  int avgSamples = 10;
  int avgIndex;

  int liveData;
  unsigned long liveDataMillis;
  unsigned long liveDataInterval;
  long breukTeller;

  long minuteData;

private:
};

class DoubleSensor
{
public:
  DoubleSensor(SimpleSensor _leftSensor, SimpleSensor _rightSensor, char _sensorName[], long _breukTeller);

  int getLiveData();
  int getMinuteData(boolean reset);

  void handle();

  void startReading(boolean left);
  void read(boolean left);
  void endReading(boolean left);
  void handleDouble();

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
