#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>

bool initTimeSync();
bool isTimeSynced();
bool syncTimeFromModem();
uint64_t getEpochMillis();
const char* getTimeSyncSource();
String getTimestampMsString();

#endif
