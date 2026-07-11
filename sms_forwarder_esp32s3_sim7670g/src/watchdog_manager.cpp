#include "watchdog_manager.h"
#include "config_manager.h"
#include "log_manager.h"
#include <vector>

WatchdogManager watchdogManager;
bool WatchdogManager::watchdog_enabled = false;
bool WatchdogManager::watchdog_initialized = false;

namespace {
std::vector<TaskHandle_t> watchedTasks;
SemaphoreHandle_t watchdogMutex = nullptr;

bool lockWatchdog() {
  return watchdogMutex &&
         xSemaphoreTake(watchdogMutex, portMAX_DELAY) == pdTRUE;
}

void unlockWatchdog() {
  xSemaphoreGive(watchdogMutex);
}

TaskHandle_t normalizeTaskHandle(TaskHandle_t taskHandle) {
  return taskHandle ? taskHandle : xTaskGetCurrentTaskHandle();
}

bool hasWatchedTask(TaskHandle_t taskHandle) {
  for (TaskHandle_t watched : watchedTasks) {
    if (watched == taskHandle) return true;
  }
  return false;
}

void rememberTask(TaskHandle_t taskHandle) {
  if (!taskHandle || hasWatchedTask(taskHandle)) return;
  watchedTasks.push_back(taskHandle);
}
}

void WatchdogManager::initWatchdog() {
  if (!watchdogMutex) {
    watchdogMutex = xSemaphoreCreateMutex();
    if (!watchdogMutex) {
      LOGE("WDT", "wdt_init_fail", "mutex");
      watchdog_enabled = false;
      return;
    }
  }
  if (!lockWatchdog()) {
    watchdog_enabled = false;
    return;
  }

  uint32_t timeout = DEFAULT_WDT_TIMEOUT;
  if (config.watchdog.timeout > 0) {
    timeout = static_cast<uint32_t>(config.watchdog.timeout);
  }
  rememberTask(xTaskGetCurrentTaskHandle());
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
      unlockWatchdog();
      return;
    }
    watchdog_initialized = true;
  }

  for (TaskHandle_t taskHandle : watchedTasks) {
    if (!taskHandle) continue;
    esp_err_t err = esp_task_wdt_add(taskHandle);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      LOGE("WDT", "wdt_task_register_fail", String((int)err).c_str());
      watchdog_enabled = false;
      unlockWatchdog();
      return;
    }
  }
  
  watchdog_enabled = true;
  LOGI("WDT", "wdt_enabled", String(timeout).c_str());
  unlockWatchdog();
}

void WatchdogManager::feedWatchdog() {
  if (!watchdogMutex || !lockWatchdog()) {
    return;
  }
  if (watchdog_enabled) {
    esp_task_wdt_reset();
  }
  unlockWatchdog();
}

void WatchdogManager::enableWatchdog() {
  if (!watchdogMutex) {
    initWatchdog();
    return;
  }
  if (!lockWatchdog()) {
    return;
  }
  if (!watchdog_initialized) {
    unlockWatchdog();
    initWatchdog();
    return;
  }
  
  if (!watchdog_enabled) {
    bool ok = true;
    for (TaskHandle_t taskHandle : watchedTasks) {
      if (!taskHandle) continue;
      esp_err_t err = esp_task_wdt_add(taskHandle);
      if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ok = false;
        LOGE("WDT", "wdt_reenable_fail", String((int)err).c_str());
        break;
      }
    }
    if (ok) {
      watchdog_enabled = true;
      LOGI("WDT", "wdt_reenabled");
    }
  }
  unlockWatchdog();
}

void WatchdogManager::disableWatchdog() {
  if (!watchdogMutex || !lockWatchdog()) {
    return;
  }
  if (!watchdog_initialized || !watchdog_enabled) {
    LOGI("WDT", "wdt_already_disabled");
    unlockWatchdog();
    return;
  }
  
  bool deleteOk = true;
  for (TaskHandle_t taskHandle : watchedTasks) {
    if (!taskHandle) continue;
    esp_err_t err = esp_task_wdt_delete(taskHandle);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      deleteOk = false;
      LOGE("WDT", "wdt_disable_fail", String((int)err).c_str());
    }
  }

  watchdog_enabled = false;

  if (!deleteOk) {
    unlockWatchdog();
    return;
  }

  esp_err_t deinitErr = esp_task_wdt_deinit();
  if (deinitErr != ESP_OK) {
    LOGE("WDT", "wdt_deinit_fail", String((int)deinitErr).c_str());
    unlockWatchdog();
    return;
  }

  watchdog_initialized = false;
  LOGI("WDT", "wdt_disabled");
  unlockWatchdog();
}

bool WatchdogManager::registerTask(TaskHandle_t taskHandle) {
  TaskHandle_t normalized = normalizeTaskHandle(taskHandle);
  if (!normalized) return false;
  if (!watchdogMutex || !lockWatchdog()) return false;

  rememberTask(normalized);

  if (!watchdog_initialized || !watchdog_enabled) {
    unlockWatchdog();
    return true;
  }

  esp_err_t err = esp_task_wdt_add(normalized);
  if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
    unlockWatchdog();
    return true;
  }

  LOGE("WDT", "wdt_task_register_fail", String((int)err).c_str());
  unlockWatchdog();
  return false;
}

void WatchdogManager::unregisterTask(TaskHandle_t taskHandle) {
  TaskHandle_t normalized = normalizeTaskHandle(taskHandle);
  if (!normalized) return;
  if (!watchdogMutex || !lockWatchdog()) return;

  for (auto it = watchedTasks.begin(); it != watchedTasks.end(); ++it) {
    if (*it == normalized) {
      watchedTasks.erase(it);
      break;
    }
  }

  if (!watchdog_initialized || !watchdog_enabled) {
    unlockWatchdog();
    return;
  }

  esp_task_wdt_delete(normalized);
  unlockWatchdog();
}

bool WatchdogManager::isEnabled() {
  if (!watchdogMutex || !lockWatchdog()) {
    return false;
  }
  bool enabled = watchdog_enabled;
  unlockWatchdog();
  return enabled;
}
