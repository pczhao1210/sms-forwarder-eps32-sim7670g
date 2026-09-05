#include "notification_manager.h"
#include <ArduinoJson.h>
#include <deque>
#include <memory>
#include "millis_utils.h"
#include "network_manager.h"
#include "http_policy.h"
#include "http_limits.h"
#include "tls_client.h"

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
  String title;
  std::shared_ptr<const Config> settings;
  bool isRetry = false;
  int smsId = 0;
  bool manual = false;
  NotificationKind kind = NotificationKind::Sms;
  int32_t reportDate = 0;
  uint32_t testId = 0;
  unsigned int systemAttempts = 0;
  uint32_t notBefore = 0;
  bool delayed = false;
};

struct NotificationResult {
  NotificationJob job;
  int successCount = 0;
  int totalCount = 0;
  float successRate = 0.0f;
  bool success = false;
  bool canceled = false;
  uint32_t finalizeAfter = 0;
  bool persistenceDeferred = false;
  uint8_t enabledMask = 0;
  uint8_t successMask = 0;
};

std::deque<NotificationJob> pendingNotificationJobs;
std::deque<NotificationResult> completedNotificationJobs;
SemaphoreHandle_t notificationQueueMutex = nullptr;
SemaphoreHandle_t notificationQueueSignal = nullptr;
TaskHandle_t notificationWorkerTaskHandle = nullptr;
bool notificationWorkerBusy = false;
bool notificationWorkerStarted = false;
int notificationWorkerSmsId = 0;
int canceledInFlightSmsId = 0;
NotificationKind notificationWorkerKind = NotificationKind::System;
int32_t notificationWorkerReportDate = 0;
NotificationTestResult latestNotificationTest;
const size_t kMaxPendingNotificationJobs = 12;

static bool isSystemNotificationActive(NotificationKind kind, int32_t reportDate) {
  if (!notificationQueueMutex) return false;
  xSemaphoreTake(notificationQueueMutex, portMAX_DELAY);
  bool active = notificationWorkerBusy && notificationWorkerKind == kind && notificationWorkerReportDate == reportDate;
  for (const auto& job : pendingNotificationJobs) active = active || (job.kind == kind && job.reportDate == reportDate);
  for (const auto& result : completedNotificationJobs) active = active || (result.job.kind == kind && result.job.reportDate == reportDate);
  xSemaphoreGive(notificationQueueMutex);
  return active;
}

static String redactUrlForLog(const String& url) {
  HttpEndpoint endpoint;
  if (!parseHttpEndpoint(url.c_str(), endpoint)) return "[invalid-url]";
  return String(endpoint.tls ? "https://" : "http://") + endpoint.host.c_str() + "/...";
}

static String summarizeContentForLog(const String& content) {
  String summary = "len=";
  summary += String(content.length());
  return summary;
}

static String normalizedBaseUrl(const String& configuredUrl, const char* fallbackUrl) {
  String baseUrl = configuredUrl;
  baseUrl.trim();
  if (baseUrl.isEmpty()) {
    baseUrl = fallbackUrl;
  }
  while (baseUrl.endsWith("/")) {
    baseUrl.remove(baseUrl.length() - 1);
  }
  return baseUrl;
}

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

static bool isNotificationJobCanceled(int smsId) {
  if (smsId <= 0 || !notificationQueueMutex) return false;
  if (xSemaphoreTake(notificationQueueMutex, pdMS_TO_TICKS(20)) != pdTRUE) return false;
  bool canceled = canceledInFlightSmsId == smsId;
  xSemaphoreGive(notificationQueueMutex);
  return canceled;
}

static NotificationResult executeNotificationJob(const NotificationJob& job) {
  NotificationResult result;
  result.job = job;
  const Config& config = *job.settings;

  auto stopIfCanceled = [&]() {
    result.canceled = isNotificationJobCanceled(job.smsId);
    return result.canceled;
  };

  if (stopIfCanceled()) return result;

  watchdogManager.feedWatchdog();

  if (job.isRetry) {
    LOGI("SMS", "sms_forward_prepare_retry", job.sender.c_str());
  } else {
    LOGI("SMS", "sms_forward_prepare", job.sender.c_str());
  }

  const String& title = job.title;

  if (config.bark.enabled) {
    result.totalCount++;
    result.enabledMask |= 1U << 0;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToBark(title, job.content, config)) { result.successCount++; result.successMask |= 1U << 0; }
  }
  if (stopIfCanceled()) return result;

  if (config.serverChan.enabled) {
    result.totalCount++;
    result.enabledMask |= 1U << 1;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToServerChan(title, job.content, config)) { result.successCount++; result.successMask |= 1U << 1; }
  }
  if (stopIfCanceled()) return result;

  if (config.telegram.enabled) {
    result.totalCount++;
    result.enabledMask |= 1U << 2;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToTelegram(title, job.content, config)) { result.successCount++; result.successMask |= 1U << 2; }
  }
  if (stopIfCanceled()) return result;

  if (config.dingtalk.enabled) {
    result.totalCount++;
    result.enabledMask |= 1U << 3;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToDingTalk(title, job.content, config)) { result.successCount++; result.successMask |= 1U << 3; }
  }
  if (stopIfCanceled()) return result;

  if (config.feishu.enabled) {
    result.totalCount++;
    result.enabledMask |= 1U << 4;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToFeishu(title, job.content, config)) { result.successCount++; result.successMask |= 1U << 4; }
  }
  if (stopIfCanceled()) return result;

  if (config.custom.enabled) {
    result.totalCount++;
    result.enabledMask |= 1U << 5;
    watchdogManager.feedWatchdog();
    if (NotificationManager::sendToCustom(title, job.content, config)) { result.successCount++; result.successMask |= 1U << 5; }
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
      auto ready = pendingNotificationJobs.begin();
      while (ready != pendingNotificationJobs.end() && ready->delayed &&
             !millisDeadlineReached(millis(), ready->notBefore)) ++ready;
      if (ready != pendingNotificationJobs.end() && completedNotificationJobs.size() < kMaxPendingNotificationJobs) {
        job = *ready;
        pendingNotificationJobs.erase(ready);
        notificationWorkerBusy = true;
        notificationWorkerSmsId = job.smsId;
        notificationWorkerKind = job.kind;
        notificationWorkerReportDate = job.reportDate;
        hasJob = true;
      } else {
        notificationWorkerBusy = false;
        notificationWorkerSmsId = 0;
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

    if (notificationQueueMutex && xSemaphoreTake(notificationQueueMutex, portMAX_DELAY) == pdTRUE) {
      if (job.smsId > 0 && canceledInFlightSmsId == job.smsId) {
        result.canceled = true;
        canceledInFlightSmsId = 0;
      }
      completedNotificationJobs.push_back(result);
      notificationWorkerBusy = false;
      notificationWorkerSmsId = 0;
      xSemaphoreGive(notificationQueueMutex);
    }
    watchdogManager.feedWatchdog();
  }
}

static bool finalizeNotificationResult(const NotificationResult& result) {
  if (result.canceled) return true;
  if (result.job.kind == NotificationKind::Test) {
    latestNotificationTest = {result.job.testId, true, result.enabledMask, result.successMask};
    return true;
  }
  if (result.job.smsId == 0) {
    if (!result.success && result.job.systemAttempts < 3) {
      NotificationJob retry = result.job;
      retry.systemAttempts++;
      retry.delayed = true;
      retry.notBefore = millisDeadlineAfter(millis(), 60000UL * retry.systemAttempts);
      if (!enqueueNotificationJob(retry)) return false;
    } else if (result.success) {
      if (result.job.kind == NotificationKind::DailyReport &&
          !statisticsManager.markDailyReportSent(result.job.reportDate)) return false;
      if (result.job.kind == NotificationKind::WeeklyReport &&
          !statisticsManager.markWeeklyReportSent(result.job.reportDate)) return false;
        if (result.job.kind == NotificationKind::RoamingAlert) networkManager.noteRoamingAlertDelivered();
    }
  }
  if (result.job.smsId > 0 && smsStorage.getSMSById(result.job.smsId).id == 0) return true;
  if (result.job.isRetry) {
    if (!retryManager.handleRetryResult(result.job.smsId, result.job.sender, result.job.content, result.success)) return false;
  } else if (result.success) {
    if (result.job.smsId > 0) {
      const char* status = result.job.manual ? SMSStatus::MANUAL_FORWARD_SUCCESS : SMSStatus::FORWARD_SUCCESS;
      if (!smsStorage.updateSMSStatus(result.job.smsId, status, getTimestampMsString(), "", -1)) return false;
      retryManager.cancelRetry(result.job.smsId);
      statisticsManager.incrementSMSForwarded();
    }
  } else if (result.job.smsId > 0) {
    if (!smsStorage.updateSMSStatus(result.job.smsId, SMSStatus::RETRY_SCHEDULED, getTimestampMsString(), "push_all_failed", -1)) return false;
    retryManager.scheduleRetry(result.job.smsId, result.job.sender, result.job.content);
  } else if (result.job.systemAttempts >= 3) {
    logManager.addLog(LOG_WARN, "PUSH", "System notification retries exhausted");
  }
  if (result.success) {
    statisticsManager.incrementPushSuccess();
  } else {
    statisticsManager.incrementPushFailed();
  }
  return true;
}
}

void NotificationManager::init() {
  ensureWorkerReady();
}

bool NotificationManager::isSMSActive(int smsId) {
  if (smsId <= 0 || !notificationQueueMutex) return false;
  if (xSemaphoreTake(notificationQueueMutex, portMAX_DELAY) != pdTRUE) return true;
  bool active = notificationWorkerBusy && notificationWorkerSmsId == smsId;
  for (const auto& job : pendingNotificationJobs) active = active || job.smsId == smsId;
  for (const auto& result : completedNotificationJobs) active = active || result.job.smsId == smsId;
  xSemaphoreGive(notificationQueueMutex);
  return active;
}

bool NotificationManager::startTest(const String& message, uint32_t& jobId) {
  if (latestNotificationTest.id != 0 && !latestNotificationTest.complete) return false;
  uint32_t nextId = latestNotificationTest.id + 1;
  if (nextId == 0) nextId = 1;
  latestNotificationTest = {nextId, false, 0, 0};
  if (!forwardSMS(i18nGet("web_test_title"), message, false, 0, false, NotificationKind::Test)) {
    latestNotificationTest.complete = true;
    return false;
  }
  jobId = nextId;
  return true;
}

bool NotificationManager::getTestResult(uint32_t jobId, NotificationTestResult& result) {
  if (jobId == 0 || jobId != latestNotificationTest.id) return false;
  result = latestNotificationTest;
  return true;
}

void NotificationManager::cancelSMS(int smsId) {
  if (smsId <= 0 || !notificationQueueMutex) return;
  if (xSemaphoreTake(notificationQueueMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

  for (auto it = pendingNotificationJobs.begin(); it != pendingNotificationJobs.end();) {
    if (it->smsId == smsId) {
      it = pendingNotificationJobs.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = completedNotificationJobs.begin(); it != completedNotificationJobs.end();) {
    if (it->job.smsId == smsId) {
      it = completedNotificationJobs.erase(it);
    } else {
      ++it;
    }
  }
  if (notificationWorkerBusy && notificationWorkerSmsId == smsId) {
    canceledInFlightSmsId = smsId;
  }
  xSemaphoreGive(notificationQueueMutex);
}

void NotificationManager::cancelAllSMS() {
  if (!notificationQueueMutex) return;
  if (xSemaphoreTake(notificationQueueMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

  for (auto it = pendingNotificationJobs.begin(); it != pendingNotificationJobs.end();) {
    if (it->smsId > 0) {
      it = pendingNotificationJobs.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = completedNotificationJobs.begin(); it != completedNotificationJobs.end();) {
    if (it->job.smsId > 0) {
      it = completedNotificationJobs.erase(it);
    } else {
      ++it;
    }
  }
  if (notificationWorkerBusy && notificationWorkerSmsId > 0) {
    canceledInFlightSmsId = notificationWorkerSmsId;
  }
  xSemaphoreGive(notificationQueueMutex);
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

  for (auto& result : completed) {
    if ((result.persistenceDeferred && !millisDeadlineReached(millis(), result.finalizeAfter)) ||
        !finalizeNotificationResult(result)) {
      if (!result.persistenceDeferred || millisDeadlineReached(millis(), result.finalizeAfter)) {
        result.persistenceDeferred = true;
        result.finalizeAfter = millisDeadlineAfter(millis(), 5000UL);
      }
      xSemaphoreTake(notificationQueueMutex, portMAX_DELAY);
      completedNotificationJobs.push_back(result);
      xSemaphoreGive(notificationQueueMutex);
    }
  }

  if (hasActiveWork) {
    setLedOverlay("working", 1500UL);
  }
}

bool NotificationManager::sendToBark(const String& title, const String& content, const Config& config) {
  if (!config.bark.enabled || config.bark.key.isEmpty()) return false;
  
  String url = config.bark.url + "/" + config.bark.key + "/" + urlEncode(title) + "/" + urlEncode(content);
  String redactedUrl = redactUrlForLog(url);
  String contentSummary = summarizeContentForLog(content);
  LOGI("BARK", "notify_url", redactedUrl.c_str());
  LOGI("BARK", "notify_content", contentSummary.c_str());
  
  bool success = sendHTTPRequest(url, "", "application/x-www-form-urlencoded", NotificationProvider::Bark, config);
  if (success) {
    LOGI("BARK", "notify_send_success");
  } else {
    LOGE("BARK", "notify_send_fail");
  }
  return success;
}

bool NotificationManager::sendToServerChan(const String& title, const String& content, const Config& config) {
  if (!config.serverChan.enabled || config.serverChan.key.isEmpty()) return false;
  
  String url = config.serverChan.url + "/" + config.serverChan.key + ".send";
  String payload = "title=" + urlEncode(title) + "&desp=" + urlEncode(content);
  String redactedUrl = redactUrlForLog(url);
  String contentSummary = summarizeContentForLog(content);
  LOGI("SERVERCHAN", "notify_url", redactedUrl.c_str());
  LOGI("SERVERCHAN", "notify_content", contentSummary.c_str());
  
  bool success = sendHTTPRequest(url, payload, "application/x-www-form-urlencoded", NotificationProvider::ServerChan, config);
  if (success) {
    LOGI("SERVERCHAN", "notify_send_success");
  } else {
    LOGE("SERVERCHAN", "notify_send_fail");
  }
  return success;
}

bool NotificationManager::sendToTelegram(const String& title, const String& content, const Config& config) {
  if (!config.telegram.enabled || config.telegram.token.isEmpty() || config.telegram.chatId.isEmpty()) return false;
  
  String url = normalizedBaseUrl(config.telegram.url, "https://api.telegram.org") + "/bot" + config.telegram.token + "/sendMessage";
  String message = title + "\n" + content;
  String payload = "chat_id=" + urlEncode(config.telegram.chatId) + "&text=" + urlEncode(message);
  return sendHTTPRequest(url, payload, "application/x-www-form-urlencoded", NotificationProvider::Telegram, config);
}

bool NotificationManager::sendToDingTalk(const String& title, const String& content, const Config& config) {
  if (!config.dingtalk.enabled || config.dingtalk.webhook.isEmpty()) return false;
  
  String payload = createJsonPayload(title, content, NotificationProvider::DingTalk);
  return !payload.isEmpty() && sendHTTPRequest(config.dingtalk.webhook, payload, "application/json", NotificationProvider::DingTalk, config);
}

bool NotificationManager::sendToFeishu(const String& title, const String& content, const Config& config) {
  if (!config.feishu.enabled || config.feishu.webhook.isEmpty()) return false;
  
  String payload = createJsonPayload(title, content);
  return !payload.isEmpty() && sendHTTPRequest(config.feishu.webhook, payload, "application/json", NotificationProvider::Feishu, config);
}

bool NotificationManager::sendToCustom(const String& title, const String& content, const Config& config) {
  if (!config.custom.enabled || config.custom.url.isEmpty()) return false;
  
  String payload = "title=" + urlEncode(title) + "&content=" + urlEncode(content);
  if (!config.custom.key.isEmpty()) {
    payload += "&key=" + urlEncode(config.custom.key);
  }
  return sendHTTPRequest(config.custom.url, payload, "application/x-www-form-urlencoded", NotificationProvider::Custom, config);
}

bool NotificationManager::forwardSMS(const String& sender, const String& content, bool isRetry, int smsId, bool manual,
                                     NotificationKind kind, int32_t reportDate) {
  sleepManager.updateActivity();
  if (smsId > 0 && isSMSActive(smsId)) return false;
  if (smsId == 0 && kind == NotificationKind::Sms) kind = NotificationKind::System;
  if (kind != NotificationKind::Sms && kind != NotificationKind::System && kind != NotificationKind::Test &&
      isSystemNotificationActive(kind, reportDate)) return true;
  if (!ensureWorkerReady()) {
    if (!isRetry && smsId > 0) retryManager.scheduleRetry(smsId, sender, content);
    return false;
  }

  NotificationJob job;
  job.sender = sender;
  job.content = content;
  job.title = i18nFormat("sms_forward_title", sender.c_str());
  job.settings = std::make_shared<const Config>(config);
  job.isRetry = isRetry;
  job.smsId = smsId;
  job.manual = manual;
  job.kind = kind;
  job.reportDate = reportDate;
  if (kind == NotificationKind::Test) job.testId = latestNotificationTest.id;

  if (enqueueNotificationJob(job)) {
    setLedOverlay("working", 1500UL);
    return true;
  }

  if (!isRetry && smsId > 0) {
    smsStorage.updateSMSStatus(smsId, SMSStatus::RETRY_SCHEDULED, getTimestampMsString(), "queue_full", -1);
    retryManager.scheduleRetry(smsId, sender, content);
  } else if (!isRetry) {
    LOGW("PUSH", "notify_queue_full_drop");
  }

  return false;
}

bool NotificationManager::sendHTTPRequest(const String& url, const String& payload, const String& contentType, NotificationProvider provider, const Config& config) {
  HttpEndpoint endpoint;
  if (!parseHttpEndpoint(url.c_str(), endpoint)) return false;
  auto feed = []() { watchdogManager.feedWatchdog(); };
  BoundedHttpClient<NetworkClient> plainClient(feed);
  BoundedHttpClient<WiFiClientSecure> secureClient(feed);
  HTTPClient http;
  if (endpoint.tls && !configureTlsClient(secureClient, endpoint.host.c_str(), config.tls.privateCaHost)) return false;
  bool started = endpoint.tls ? http.begin(secureClient, url) : http.begin(plainClient, url);
  if (!started) return false;
  http.addHeader("Content-Type", contentType);
  http.setConnectTimeout(2000);
  http.setTimeout(2000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  
  int httpCode;
  if (payload.isEmpty()) {
    httpCode = http.GET();
  } else {
    httpCode = http.POST(payload);
  }
  
  BoundedHttpResponse response;
  bool complete = httpCode > 0 && http.getSize() <= 4096 && http.writeToStream(&response) >= 0 && response.complete() &&
                  !plainClient.limitExceeded() && !secureClient.limitExceeded();
  DynamicJsonDocument document(4096);
  DeserializationError error = deserializeJson(document, response.body());
  bool success = complete && (provider == NotificationProvider::Custom || !error) &&
                 notificationResponseSucceeded(provider, httpCode, document.as<JsonVariantConst>());
  if (!success) {
    LOGE("HTTP", "http_error",
         String(httpCode).c_str(),
         http.errorToString(httpCode).c_str(),
         complete ? "provider_rejected" : "response_incomplete_or_too_large");
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

String NotificationManager::createJsonPayload(const String& title, const String& content, NotificationProvider provider) {
  DynamicJsonDocument doc(1024 + title.length() + content.length() * 2);
  populateTextNotificationPayload(doc, provider, title, content);
  if (doc.overflowed()) return "";
  String payload;
  serializeJson(doc, payload);
  return payload;
}
