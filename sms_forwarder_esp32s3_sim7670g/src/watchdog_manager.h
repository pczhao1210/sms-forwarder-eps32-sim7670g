#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#include <cstdint>
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class WatchdogManager {
public:
  static void initWatchdog();
  static void feedWatchdog();
  static void enableWatchdog();
  static void disableWatchdog();
  static bool registerTask(TaskHandle_t taskHandle = nullptr);
  static void unregisterTask(TaskHandle_t taskHandle = nullptr);
  static bool isEnabled();
  
private:
  enum class LifecycleState : uint8_t {
    Uninitialized,
    Enabled,
    Disabled,
    InitializationFailed
  };

  static bool initWatchdogLocked();
  static LifecycleState lifecycleState;
  static const uint32_t DEFAULT_WDT_TIMEOUT = 30; // 30秒超时
};

extern WatchdogManager watchdogManager;

#endif
