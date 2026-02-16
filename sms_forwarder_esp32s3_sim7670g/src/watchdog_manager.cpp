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
      LOGE("WDT", "wdt_init_fail", String((int)err).c_str());
      watchdog_enabled = false;
      return;
    }
    
    err = esp_task_wdt_add(NULL);
    if (err != ESP_OK) {
      LOGE("WDT", "wdt_task_register_fail", String((int)err).c_str());
      watchdog_enabled = false;
      return;
    }
    watchdog_initialized = true;
  }
  
  watchdog_enabled = true;
  LOGI("WDT", "wdt_enabled", String(timeout).c_str());
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
      LOGI("WDT", "wdt_reenabled");
    } else {
      LOGE("WDT", "wdt_reenable_fail", String((int)err).c_str());
    }
  }
}

void WatchdogManager::disableWatchdog() {
  if (!watchdog_initialized || !watchdog_enabled) {
    LOGI("WDT", "wdt_already_disabled");
    return;
  }
  
  esp_err_t err = esp_task_wdt_delete(NULL);
  if (err == ESP_OK) {
    watchdog_enabled = false;
    esp_err_t deinitErr = esp_task_wdt_deinit();
    if (deinitErr != ESP_OK) {
      LOGE("WDT", "wdt_deinit_fail", String((int)deinitErr).c_str());
      return;
    }
    watchdog_initialized = false;
    LOGI("WDT", "wdt_disabled");
  } else {
    LOGE("WDT", "wdt_disable_fail", String((int)err).c_str());
  }
}
