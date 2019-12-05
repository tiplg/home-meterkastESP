#ifndef Sens_h
#define Sens_h

#include "Arduino.h"
#include <PubSubClient.h>

class SimpleSensor
{
public:
  SimpleSensor(int _pin, int _thresholdSet, int _thresholdReset, int _timeout, char _sensorName[], long _breukTeller);

  int getSensorData();

  void makeHigh();
  void makeInput();
  boolean checkInput();
  void checkThreshold();
  void publishMinuteData(PubSubClient MQTTclient);
  void publishLiveData(PubSubClient MQTTclient);
  void handle();
  void ISR();

  int sensorPin;
  char sensorName[];
  int thresholdSet;
  int thresholdReset;
  int timeout;
  boolean attached;
  boolean interrupted;

  int samples = 0;

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

#endif
