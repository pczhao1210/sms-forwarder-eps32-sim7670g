#include "watchdog_manager.h"
#include "config_manager.h"
#include "log_manager.h"
#include <vector>

WatchdogManager watchdogManager;
WatchdogManager::LifecycleState WatchdogManager::lifecycleState =
    WatchdogManager::LifecycleState::Uninitialized;

namespace {
std::vector<TaskHandle_t> watchedTasks;
SemaphoreHandle_t watchdogMutex = nullptr;
StaticSemaphore_t watchdogMutexBuffer;
portMUX_TYPE watchdogMutexInitLock = portMUX_INITIALIZER_UNLOCKED;

bool ensureWatchdogMutex() {
  portENTER_CRITICAL(&watchdogMutexInitLock);
  if (!watchdogMutex) {
    watchdogMutex = xSemaphoreCreateMutexStatic(&watchdogMutexBuffer);
  }
  bool ready = watchdogMutex != nullptr;
  portEXIT_CRITICAL(&watchdogMutexInitLock);
  return ready;
}

bool lockWatchdog() {
  return ensureWatchdogMutex() &&
         xSemaphoreTake(watchdogMutex, portMAX_DELAY) == pdTRUE;
}

void unlockWatchdog() {
  xSemaphoreGive(watchdogMutex);
}

class WatchdogLock {
public:
  WatchdogLock() : locked(lockWatchdog()) {}
  ~WatchdogLock() {
    if (locked) unlockWatchdog();
  }

  explicit operator bool() const {
    return locked;
  }

private:
  bool locked;
};

bool isSuccessfulSubscriptionResult(esp_err_t err) {
  return err == ESP_OK || err == ESP_ERR_INVALID_STATE;
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
  WatchdogLock lock;
  if (!lock) {
    LOGE("WDT", "wdt_init_fail", "mutex");
    return;
  }

  if (initWatchdogLocked()) {
    uint32_t timeout = config.watchdog.timeout > 0
                           ? static_cast<uint32_t>(config.watchdog.timeout)
                           : DEFAULT_WDT_TIMEOUT;
    LOGI("WDT", "wdt_enabled", String(timeout).c_str());
  }
}

bool WatchdogManager::initWatchdogLocked() {
  uint32_t timeout = DEFAULT_WDT_TIMEOUT;
  if (config.watchdog.timeout > 0) {
    timeout = static_cast<uint32_t>(config.watchdog.timeout);
  }
  rememberTask(xTaskGetCurrentTaskHandle());

  if (lifecycleState == LifecycleState::Enabled) {
    return true;
  }

  esp_task_wdt_config_t wdtConfig = {};
  wdtConfig.timeout_ms = timeout * 1000;
  wdtConfig.idle_core_mask = 0;
  wdtConfig.trigger_panic = true;

  esp_err_t err = esp_task_wdt_init(&wdtConfig);
  if (err == ESP_ERR_INVALID_STATE) {
    esp_err_t deinitErr = esp_task_wdt_deinit();
    if (deinitErr != ESP_OK && deinitErr != ESP_ERR_INVALID_STATE) {
      LOGE("WDT", "wdt_deinit_fail", String((int)deinitErr).c_str());
      lifecycleState = LifecycleState::InitializationFailed;
      return false;
    }
    err = esp_task_wdt_init(&wdtConfig);
  }

  if (err != ESP_OK) {
    LOGE("WDT", "wdt_init_fail", String((int)err).c_str());
    lifecycleState = LifecycleState::InitializationFailed;
    return false;
  }

  for (TaskHandle_t taskHandle : watchedTasks) {
    esp_err_t addErr = esp_task_wdt_add(taskHandle);
    if (!isSuccessfulSubscriptionResult(addErr)) {
      LOGE("WDT", "wdt_task_register_fail", String((int)addErr).c_str());
      for (TaskHandle_t registeredTask : watchedTasks) {
        esp_task_wdt_delete(registeredTask);
      }
      esp_err_t deinitErr = esp_task_wdt_deinit();
      if (deinitErr != ESP_OK && deinitErr != ESP_ERR_INVALID_STATE) {
        LOGE("WDT", "wdt_deinit_fail", String((int)deinitErr).c_str());
      }
      lifecycleState = LifecycleState::InitializationFailed;
      return false;
    }
  }

  lifecycleState = LifecycleState::Enabled;
  return true;
}

void WatchdogManager::feedWatchdog() {
  WatchdogLock lock;
  if (!lock) return;

  if (lifecycleState == LifecycleState::Enabled) {
    esp_task_wdt_reset();
  }
}

void WatchdogManager::enableWatchdog() {
  WatchdogLock lock;
  if (!lock || lifecycleState == LifecycleState::Enabled) return;

  if (initWatchdogLocked()) {
    LOGI("WDT", "wdt_reenabled");
  }
}

void WatchdogManager::disableWatchdog() {
  WatchdogLock lock;
  if (!lock) return;

  if (lifecycleState == LifecycleState::Disabled) {
    LOGI("WDT", "wdt_already_disabled");
    return;
  }

  if (lifecycleState == LifecycleState::Uninitialized) {
    lifecycleState = LifecycleState::Disabled;
    LOGI("WDT", "wdt_already_disabled");
    return;
  }

  LifecycleState previousState = lifecycleState;
  for (TaskHandle_t taskHandle : watchedTasks) {
    esp_err_t err = esp_task_wdt_delete(taskHandle);
    if (!isSuccessfulSubscriptionResult(err)) {
      LOGE("WDT", "wdt_disable_fail", String((int)err).c_str());
    }
  }

  esp_err_t deinitErr = esp_task_wdt_deinit();
  if (deinitErr != ESP_OK && deinitErr != ESP_ERR_INVALID_STATE) {
    LOGE("WDT", "wdt_deinit_fail", String((int)deinitErr).c_str());
    if (previousState == LifecycleState::Enabled) {
      for (TaskHandle_t taskHandle : watchedTasks) {
        esp_err_t addErr = esp_task_wdt_add(taskHandle);
        if (!isSuccessfulSubscriptionResult(addErr)) {
          LOGE("WDT", "wdt_reenable_fail", String((int)addErr).c_str());
        }
      }
    }
    return;
  }

  lifecycleState = LifecycleState::Disabled;
  LOGI("WDT", "wdt_disabled");
}

bool WatchdogManager::registerTask(TaskHandle_t taskHandle) {
  WatchdogLock lock;
  if (!lock) return false;

  TaskHandle_t normalized = normalizeTaskHandle(taskHandle);
  if (!normalized) return false;

  rememberTask(normalized);

  if (lifecycleState != LifecycleState::Enabled) {
    return true;
  }

  esp_err_t err = esp_task_wdt_add(normalized);
  if (isSuccessfulSubscriptionResult(err)) {
    return true;
  }

  LOGE("WDT", "wdt_task_register_fail", String((int)err).c_str());
  return false;
}

void WatchdogManager::unregisterTask(TaskHandle_t taskHandle) {
  WatchdogLock lock;
  if (!lock) return;

  TaskHandle_t normalized = normalizeTaskHandle(taskHandle);
  if (!normalized) return;

  for (auto it = watchedTasks.begin(); it != watchedTasks.end(); ++it) {
    if (*it == normalized) {
      watchedTasks.erase(it);
      break;
    }
  }

  if (lifecycleState == LifecycleState::Enabled) {
    esp_task_wdt_delete(normalized);
  }
}

bool WatchdogManager::isEnabled() {
  WatchdogLock lock;
  return lock && lifecycleState == LifecycleState::Enabled;
}
