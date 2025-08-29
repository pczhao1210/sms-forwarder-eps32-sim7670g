#include "watchdog_manager.h"
#include "log_manager.h"

WatchdogManager watchdogManager;
bool WatchdogManager::watchdog_enabled = false;

void WatchdogManager::initWatchdog() {
  // 完全禁用看门狗功能
  watchdog_enabled = false;
  logManager.addLog(LOG_INFO, "WDT", "看门狗已禁用");
}

void WatchdogManager::feedWatchdog() {
  // 简化处理，直接忽略看门狗错误
  if (watchdog_enabled) {
    esp_task_wdt_reset();
  }
}

void WatchdogManager::enableWatchdog() {
  // 禁用看门狗启用功能
  logManager.addLog(LOG_INFO, "WDT", "看门狗功能已禁用");
}

void WatchdogManager::disableWatchdog() {
  watchdog_enabled = false;
  logManager.addLog(LOG_INFO, "WDT", "看门狗已禁用");
}