#include "watchdog_manager.h"
#include "config_manager.h"
#include "log_manager.h"

WatchdogManager watchdogManager;
bool WatchdogManager::watchdog_enabled = false;
bool WatchdogManager::watchdog_initialized = false;

void WatchdogManager::initWatchdog() {
  uint32_t timeout = DEFAULT_WDT_TIMEOUT;
  if (config.watchdog.timeout > 0) {
    timeout = static_cast<uint32_t>(config.watchdog.timeout);
  }
  esp_task_wdt_config_t wdtConfig = {};
  wdtConfig.timeout_ms = timeout * 1000;
  wdtConfig.idle_core_mask = 0;
  wdtConfig.trigger_panic = true;
  if (!watchdog_initialized) {
    esp_err_t err = esp_task_wdt_init(&wdtConfig);
    if (err == ESP_ERR_INVALID_STATE) {
      esp_task_wdt_deinit();
      err = esp_task_wdt_init(&wdtConfig);
    }
    
    if (err != ESP_OK) {
      logManager.addLog(LOG_ERROR, "WDT", "初始化失败, err=" + String((int)err));
      watchdog_enabled = false;
      return;
    }
    
    err = esp_task_wdt_add(NULL);
    if (err != ESP_OK) {
      logManager.addLog(LOG_ERROR, "WDT", "任务注册失败, err=" + String((int)err));
      watchdog_enabled = false;
      return;
    }
    watchdog_initialized = true;
  }
  
  watchdog_enabled = true;
  logManager.addLog(LOG_INFO, "WDT", "看门狗已启用, timeout=" + String(timeout) + "s");
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
    esp_err_t err = esp_task_wdt_add(NULL);
    if (err == ESP_OK) {
      watchdog_enabled = true;
      logManager.addLog(LOG_INFO, "WDT", "看门狗已重新启用");
    } else {
      logManager.addLog(LOG_ERROR, "WDT", "重新启用失败, err=" + String((int)err));
    }
  }
}

void WatchdogManager::disableWatchdog() {
  if (!watchdog_initialized || !watchdog_enabled) {
    logManager.addLog(LOG_INFO, "WDT", "看门狗已处于关闭状态");
    return;
  }
  
  esp_err_t err = esp_task_wdt_delete(NULL);
  if (err == ESP_OK) {
    watchdog_enabled = false;
    esp_err_t deinitErr = esp_task_wdt_deinit();
    if (deinitErr != ESP_OK) {
      logManager.addLog(LOG_ERROR, "WDT", "deinit失败, err=" + String((int)deinitErr));
      return;
    }
    watchdog_initialized = false;
    logManager.addLog(LOG_INFO, "WDT", "看门狗已关闭");
  } else {
    logManager.addLog(LOG_ERROR, "WDT", "关闭失败, err=" + String((int)err));
  }
}
