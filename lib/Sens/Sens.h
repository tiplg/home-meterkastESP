#ifndef Sens_h
#define Sens_h

#include "Arduino.h"
#include <PubSubClient.h>

class SimpleSensor
{
public:
  SimpleSensor();
  SimpleSensor(int _pin, int _thresholdSet, int _thresholdReset, int _timeout);
  SimpleSensor(int _pin, int _thresholdSet, int _thresholdReset, int _timeout, char _sensorName[], long _breukTeller);

  int getSensorData();

  void makeHigh();
  void makeInput();
  boolean checkInput();
  boolean checkThreshold();
  void publishMinuteData(PubSubClient MQTTclient);
  void publishLiveData(PubSubClient MQTTclient);
  boolean handle();
  void ISR();

  int sensorPin;
  char sensorName[];
  int thresholdSet;
  int thresholdReset;
  int timeout;

  int samples = 0;

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
    MAKEHIGH,
    MAKEINPUT,
    CHECKINPUT
  };

  enum states state;

  long stateTime;
};

class DoubleSensor
{
public:
  DoubleSensor(SimpleSensor _leftSensor, SimpleSensor _rightSensor);

  void handle();

  SimpleSensor leftSensor;
  SimpleSensor rightSensor;

  int sensorData;

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
    TRIGGEREDLEFT,
    TRIGGEREDRIGHT,
    ARMED,
    ARMEDLEFT,
    ARMEDRIGHT,
    FIRED,
    FIREDLEFT,
    FIREDRIGHT,
  };

  enum states state;
};

#endif
