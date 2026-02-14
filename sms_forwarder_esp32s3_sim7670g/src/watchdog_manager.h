#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#include <cstdint>
#include <Arduino.h>
#include <esp_task_wdt.h>

class WatchdogManager {
public:
  static void initWatchdog();
  static void feedWatchdog();
  static void enableWatchdog();
  static void disableWatchdog();
  
private:
  static bool watchdog_enabled;
  static bool watchdog_initialized;
  static const uint32_t DEFAULT_WDT_TIMEOUT = 30; // 30秒超时
};

extern WatchdogManager watchdogManager;

#endif
