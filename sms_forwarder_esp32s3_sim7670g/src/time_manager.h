#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>

bool initTimeSync();
bool isTimeSynced();
bool syncTimeFromModem();
void pollTimeSyncRecovery();
uint64_t getEpochMillis();
int getConfiguredTimezoneOffsetMinutes();
int getConfiguredLocalMinuteOfDay();
bool getConfiguredLocalTime(struct tm& timeinfo);
const char* getTimeSyncSource();
String getTimestampMsString();

#endif
