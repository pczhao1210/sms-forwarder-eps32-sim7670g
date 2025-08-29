#ifndef STATISTICS_MANAGER_H
#define STATISTICS_MANAGER_H

#include <Arduino.h>

struct Statistics {
  uint32_t totalSMSReceived;
  uint32_t totalSMSForwarded;
  uint32_t totalSMSFiltered;
  uint32_t totalPushSuccess;
  uint32_t totalPushFailed;
  uint32_t totalRetries;
  uint32_t uptime;
  String lastSMSTime;
  String lastSender;
};

class StatisticsManager {
public:
  static void incrementSMSReceived();
  static void incrementSMSForwarded();
  static void incrementSMSFiltered();
  static void incrementPushSuccess();
  static void incrementPushFailed();
  static void incrementRetries();
  static void updateLastSMS(const String& sender);
  static Statistics getStatistics();
  static void resetStatistics();
  static void saveStatistics();
  static void loadStatistics();
  
private:
  static Statistics stats;
  static unsigned long startTime;
};

extern StatisticsManager statisticsManager;

#endif