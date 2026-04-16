#include "notification_manager.h"
#include <ArduinoJson.h>
#include <deque>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "config_manager.h"
#include "log_manager.h"
#include "led_controller.h"
#include "statistics_manager.h"
#include "retry_manager.h"
#include "watchdog_manager.h"
#include "battery_manager.h"
#include "i18n.h"
#include "sms_storage.h"
#include "time_manager.h"

NotificationManager notificationManager;

namespace {
struct NotificationJob {
  String sender;
  String content;
  bool isRetry = false;
  int smsId = 0;
  bool manual = false;
};

struct NotificationResult {
  NotificationJob job;
  int successCount = 0;
  int totalCount = 0;
  float successRate = 0.0f;
  bool success = false;
};

std::deque<NotificationJob> pendingNotificationJobs;
std::deque<NotificationResult> completedNotificationJobs;
SemaphoreHandle_t notificationQueueMutex = nullptr;
SemaphoreHandle_t notificationQueueSignal = nullptr;
TaskHandle_t notificationWorkerTaskHandle = nullptr;
bool notificationWorkerBusy = false;
bool notificationWorkerStarted = false;
const size_t kMaxPendingNotificationJobs = 12;

static bool enqueueNotificationJob(const NotificationJob& job) {
  if (!notificationQueueMutex) return false;
  if (xSemaphoreTake(notificationQueueMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
    return false;
  }

  bool accepted = pendingNotificationJobs.size() < kMaxPendingNotificationJobs;
  if (accepted) {
    pendingNotificationJobs.push_back(job);
  }
  xSemaphoreGive(notificationQueueMutex);

  if (accepted && notificationQueueSignal) {
    xSemaphoreGive(notificationQueueSignal);
  }
  return accepted;
}

static NotificationResult executeNotificationJob(const NotificationJob& job) {
  NotificationResult result;
  result.job = job;

  watchdogManager.feedWatchdog();

  if (job.isRetry) {
    LOGI("SMS", "sms_forward_prepare_retry", job.sender.c_str());
  } else {
    LOGI("SMS", "sms_forward_prepare", job.sender.c_str());
  }

  String title = i18nFormat("sms_forward_title", job.sender.c_str());

  if (config.bark.enabled) {
    result.totalCount++;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToBark(title, job.content)) result.successCount++;
  }

  if (config.serverChan.enabled) {
    result.totalCount++;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToServerChan(title, job.content)) result.successCount++;
  }

  if (config.telegram.enabled) {
    result.totalCount++;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToTelegram(title, job.content)) result.successCount++;
  }

  if (config.dingtalk.enabled) {
    result.totalCount++;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToDingTalk(title, job.content)) result.successCount++;
  }

  if (config.feishu.enabled) {
    result.totalCount++;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToFeishu(title, job.content)) result.successCount++;
  }

  if (config.custom.enabled) {
    result.totalCount++;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToCustom(title, job.content)) result.successCount++;
  }

  watchdogManager.feedWatchdog();
  result.successRate = result.totalCount > 0 ? (float)result.successCount / result.totalCount * 100.0f : 0.0f;
  result.success = result.successCount > 0;
  return result;
}

static void notificationWorkerTask(void* parameter) {
  (void)parameter;
  watchdogManager.registerTask(xTaskGetCurrentTaskHandle());

  for (;;) {
    NotificationJob job;
    bool hasJob = false;

    if (notificationQueueMutex && xSemaphoreTake(notificationQueueMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (!pendingNotificationJobs.empty()) {
        job = pendingNotificationJobs.front();
        pendingNotificationJobs.pop_front();
        notificationWorkerBusy = true;
        hasJob = true;
      } else {
        notificationWorkerBusy = false;
      }
      xSemaphoreGive(notificationQueueMutex);
    }

    if (!hasJob) {
      watchdogManager.feedWatchdog();
      if (notificationQueueSignal) {
        xSemaphoreTake(notificationQueueSignal, pdMS_TO_TICKS(250));
      } else {
        vTaskDelay(pdMS_TO_TICKS(250));
      }
      continue;
    }

    NotificationResult result = executeNotificationJob(job);

    if (notificationQueueMutex && xSemaphoreTake(notificationQueueMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      completedNotificationJobs.push_back(result);
      notificationWorkerBusy = false;
      xSemaphoreGive(notificationQueueMutex);
    }
    watchdogManager.feedWatchdog();
  }
}

static void finalizeNotificationResult(const NotificationResult& result) {
  if (result.success) {
    LOGI("PUSH", "push_success_rate",
         String(result.successCount).c_str(),
         String(result.totalCount).c_str(),
         String(result.successRate, 1).c_str());
    statisticsManager.incrementPushSuccess();
  } else {
    LOGE("PUSH", "push_all_failed");
    statisticsManager.incrementPushFailed();
  }

  if (result.job.isRetry) {
    retryManager.handleRetryResult(result.job.smsId, result.job.sender, result.job.content, result.success);
    if (!result.success) {
      LOGW("RETRY", "retry_still_failed");
    }
    return;
  }

  if (result.success) {
    if (result.job.smsId > 0) {
      retryManager.cancelRetry(result.job.smsId);
      const char* status = result.job.manual ? SMSStatus::MANUAL_FORWARD_SUCCESS : SMSStatus::FORWARD_SUCCESS;
      smsStorage.updateSMSStatus(result.job.smsId, status, getTimestampMsString(), "", -1);
      statisticsManager.incrementSMSForwarded();
    }
    return;
  }

  if (result.job.smsId > 0) {
    smsStorage.updateSMSStatus(result.job.smsId, SMSStatus::RETRY_SCHEDULED, getTimestampMsString(), "push_all_failed", -1);
  }
  retryManager.scheduleRetry(result.job.smsId, result.job.sender, result.job.content);
}
}

void NotificationManager::init() {
  ensureWorkerReady();
}

bool NotificationManager::ensureWorkerReady() {
  if (notificationWorkerStarted) return true;

  if (!notificationQueueMutex) {
    notificationQueueMutex = xSemaphoreCreateMutex();
  }
  if (!notificationQueueSignal) {
    notificationQueueSignal = xSemaphoreCreateBinary();
  }
  if (!notificationQueueMutex || !notificationQueueSignal) {
    return false;
  }

  BaseType_t created = xTaskCreatePinnedToCore(
    notificationWorkerTask,
    "notify_worker",
    12288,
    nullptr,
    1,
    &notificationWorkerTaskHandle,
    1
  );

  notificationWorkerStarted = (created == pdPASS);
  return notificationWorkerStarted;
}

void NotificationManager::processQueue() {
  if (!ensureWorkerReady()) return;

  std::deque<NotificationResult> completed;
  bool hasActiveWork = false;
  if (notificationQueueMutex && xSemaphoreTake(notificationQueueMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    completed.swap(completedNotificationJobs);
    hasActiveWork = notificationWorkerBusy || !pendingNotificationJobs.empty();
    xSemaphoreGive(notificationQueueMutex);
  }

  for (const auto& result : completed) {
    finalizeNotificationResult(result);
  }

  if (hasActiveWork) {
    setLedOverlay("working", 1500UL);
  }
}

bool NotificationManager::sendToBark(const String& title, const String& content) {
  if (!config.bark.enabled || config.bark.key.isEmpty()) return false;
  
  String url = config.bark.url + "/" + config.bark.key + "/" + urlEncode(title) + "/" + urlEncode(content);
  LOGI("BARK", "notify_url", url.c_str());
  LOGI("BARK", "notify_content", content.c_str());
  
  bool success = sendHTTPRequest(url);
  if (success) {
    LOGI("BARK", "notify_send_success");
  } else {
    LOGE("BARK", "notify_send_fail");
  }
  return success;
}

bool NotificationManager::sendToServerChan(const String& title, const String& content) {
  if (!config.serverChan.enabled || config.serverChan.key.isEmpty()) return false;
  
  String url = config.serverChan.url + "/" + config.serverChan.key + ".send";
  String payload = "title=" + urlEncode(title) + "&desp=" + urlEncode(content);
  LOGI("SERVERCHAN", "notify_url", url.c_str());
  LOGI("SERVERCHAN", "notify_content", content.c_str());
  
  bool success = sendHTTPRequest(url, payload);
  if (success) {
    LOGI("SERVERCHAN", "notify_send_success");
  } else {
    LOGE("SERVERCHAN", "notify_send_fail");
  }
  return success;
}

bool NotificationManager::sendToTelegram(const String& title, const String& content) {
  if (!config.telegram.enabled || config.telegram.token.isEmpty()) return false;
  
  String url = "https://api.telegram.org/bot" + config.telegram.token + "/sendMessage";
  String message = title + "\n" + content;
  String payload = "chat_id=" + config.telegram.chatId + "&text=" + urlEncode(message);
  return sendHTTPRequest(url, payload);
}

bool NotificationManager::sendToDingTalk(const String& title, const String& content) {
  if (!config.dingtalk.enabled || config.dingtalk.webhook.isEmpty()) return false;
  
  String payload = createJsonPayload(title, content);
  return sendHTTPRequest(config.dingtalk.webhook, payload, "application/json");
}

bool NotificationManager::sendToFeishu(const String& title, const String& content) {
  if (!config.feishu.enabled || config.feishu.webhook.isEmpty()) return false;
  
  String payload = createJsonPayload(title, content);
  return sendHTTPRequest(config.feishu.webhook, payload, "application/json");
}

bool NotificationManager::sendToCustom(const String& title, const String& content) {
  if (!config.custom.enabled || config.custom.url.isEmpty()) return false;
  
  String payload = "title=" + urlEncode(title) + "&content=" + urlEncode(content);
  return sendHTTPRequest(config.custom.url, payload);
}

bool NotificationManager::forwardSMS(const String& sender, const String& content, bool isRetry, int smsId, bool manual) {
  sleepManager.updateActivity();
  if (!ensureWorkerReady()) {
    return false;
  }

  NotificationJob job;
  job.sender = sender;
  job.content = content;
  job.isRetry = isRetry;
  job.smsId = smsId;
  job.manual = manual;

  if (enqueueNotificationJob(job)) {
    setLedOverlay("working", 1500UL);
    return true;
  }

  if (!isRetry) {
    if (smsId > 0) {
      smsStorage.updateSMSStatus(smsId, SMSStatus::RETRY_SCHEDULED, getTimestampMsString(), "queue_full", -1);
    }
    retryManager.scheduleRetry(smsId, sender, content);
  }

  return false;
}

bool NotificationManager::sendHTTPRequest(const String& url, const String& payload, const String& contentType) {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  http.begin(client, url);
  http.addHeader("Content-Type", contentType);
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode;
  if (payload.isEmpty()) {
    httpCode = http.GET();
  } else {
    httpCode = http.POST(payload);
  }
  
  String response = http.getString();
  bool success = (httpCode >= 200 && httpCode < 300);
  if (!success) {
    String snippet = response.length() > 200 ? response.substring(0, 200) : response;
    LOGE("HTTP", "http_error",
         String(httpCode).c_str(),
         http.errorToString(httpCode).c_str(),
         snippet.c_str());
  }
  http.end();
  
  return success;
}

String NotificationManager::urlEncode(const String& str) {
  String encoded = "";
  for (int i = 0; i < str.length(); i++) {
    unsigned char uc = static_cast<unsigned char>(str.charAt(i));
    
    if (isalnum(uc) || uc == '-' || uc == '_' || uc == '.' || uc == '~') {
      encoded += static_cast<char>(uc);
    } else {
      encoded += "%";
      if (uc < 16) encoded += "0";
      String hex = String(uc, HEX);
      hex.toUpperCase();
      encoded += hex;
    }
  }
  return encoded;
}

String NotificationManager::createJsonPayload(const String& title, const String& content) {
  DynamicJsonDocument doc(1024 + title.length() + content.length() * 2);
  doc["msg_type"] = "text";
  JsonObject body = doc.createNestedObject("content");
  body["text"] = title + "\n" + content;
  String payload;
  serializeJson(doc, payload);
  return payload;
}
