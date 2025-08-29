#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#include <cstdint>
#include <esp_task_wdt.h>

class WatchdogManager {
public:
  static void initWatchdog();
  static void feedWatchdog();
  static void enableWatchdog();
  static void disableWatchdog();
  
private:
  static bool watchdog_enabled;
  static const uint32_t WDT_TIMEOUT = 30; // 30秒超时
};

extern WatchdogManager watchdogManager;

#endif