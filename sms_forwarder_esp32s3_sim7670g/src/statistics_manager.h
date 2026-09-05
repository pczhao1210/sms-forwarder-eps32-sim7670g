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
  int32_t lastDailyReportDate;
  int32_t lastWeeklyReportDate;
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
  static bool wasDailyReportSent(int32_t dateKey);
  static bool wasWeeklyReportSent(int32_t dateKey);
  static bool markDailyReportSent(int32_t dateKey);
  static bool markWeeklyReportSent(int32_t dateKey);
  static bool resetStatistics();
  static bool saveStatistics();
  static void processPersistence();
  static void loadStatistics();
  
private:
  static Statistics stats;
};

extern StatisticsManager statisticsManager;

#endif