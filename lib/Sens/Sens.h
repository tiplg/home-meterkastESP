#ifndef Sens_h
#define Sens_h

#include "Arduino.h"
#include <PubSubClient.h>

class SimpleSensor
{
public:
  SimpleSensor(int _pin, int _thresholdSet, int _thresholdReset, char _sensorName[], long _breukTeller);

  void makeHigh();
  void makeInput();
  void checkInput(int ticks);
  void checkThreshold();
  void publishMinuteData(PubSubClient MQTTclient);
  void publishLiveData(PubSubClient MQTTclient);

  int sensorPin;
  char sensorName[32];
  int thresholdSet;
  int thresholdReset;
  int sensorData;
  int sensorMin;
  int sensorMax;
  bool armed;

  int liveData;
  unsigned long liveTimestamp;
  unsigned long interval;
  long breukTeller;
  char liveTopic[32];

  long minuteData;
  char minuteTopic[32];
};

#endif
