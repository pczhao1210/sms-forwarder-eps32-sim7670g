#include "watchdog_manager.h"
#include "log_manager.h"

WatchdogManager watchdogManager;
bool WatchdogManager::watchdog_enabled = false;
bool WatchdogManager::watchdog_initialized = false;

void WatchdogManager::initWatchdog() {
  if (!watchdog_initialized) {
    esp_err_t err = esp_task_wdt_init(WDT_TIMEOUT, true);
    if (err == ESP_ERR_INVALID_STATE) {
      esp_task_wdt_deinit();
      err = esp_task_wdt_init(WDT_TIMEOUT, true);
    }
    
    if (err != ESP_OK) {
      logManager.addLog(LOG_ERROR, "WDT", "初始化失败, err=" + String((int)err));
      watchdog_enabled = false;
      return;
    }
    
    esp_task_wdt_add(NULL);
    watchdog_initialized = true;
  }
  
  watchdog_enabled = true;
  logManager.addLog(LOG_INFO, "WDT", "看门狗已启用, timeout=" + String(WDT_TIMEOUT) + "s");
}

void WatchdogManager::feedWatchdog() {
  if (watchdog_enabled) {
    esp_task_wdt_reset();
  }
}

void WatchdogManager::enableWatchdog() {
  if (!watchdog_initialized) {
    initWatchdog();
    return;
  }
  
  if (!watchdog_enabled) {
    esp_task_wdt_add(NULL);
    watchdog_enabled = true;
    logManager.addLog(LOG_INFO, "WDT", "看门狗已重新启用");
  }
}

void WatchdogManager::disableWatchdog() {
  if (!watchdog_initialized || !watchdog_enabled) {
    logManager.addLog(LOG_INFO, "WDT", "看门狗已处于关闭状态");
    return;
  }
  
  esp_task_wdt_delete(NULL);
  watchdog_enabled = false;
  logManager.addLog(LOG_INFO, "WDT", "看门狗已关闭");
}
