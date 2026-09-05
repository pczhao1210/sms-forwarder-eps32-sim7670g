#ifndef NOTIFICATION_TYPES_H
#define NOTIFICATION_TYPES_H

#include <cstdint>

enum class NotificationKind { Sms, System, DailyReport, WeeklyReport, RoamingAlert, Test };

struct NotificationTestResult {
  uint32_t id = 0;
  bool complete = false;
  uint8_t enabledMask = 0;
  uint8_t successMask = 0;
};

#endif