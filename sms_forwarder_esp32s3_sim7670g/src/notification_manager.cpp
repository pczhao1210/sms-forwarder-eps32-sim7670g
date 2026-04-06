#include "notification_manager.h"
#include <ArduinoJson.h>

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
  setStatusLED("working");
  if (isRetry) {
    LOGI("SMS", "sms_forward_prepare_retry", sender.c_str());
  } else {
    LOGI("SMS", "sms_forward_prepare", sender.c_str());
  }
  
  String title = i18nFormat("sms_forward_title", sender.c_str());
  int successCount = 0;
  int totalCount = 0;
  
  if (config.bark.enabled) {
    totalCount++;
    watchdogManager.feedWatchdog();
    if (sendToBark(title, content)) successCount++;
  }
  
  if (config.serverChan.enabled) {
    totalCount++;
    watchdogManager.feedWatchdog();
    if (sendToServerChan(title, content)) successCount++;
  }
  
  if (config.telegram.enabled) {
    totalCount++;
    watchdogManager.feedWatchdog();
    if (sendToTelegram(title, content)) successCount++;
  }
  
  if (config.dingtalk.enabled) {
    totalCount++;
    watchdogManager.feedWatchdog();
    if (sendToDingTalk(title, content)) successCount++;
  }
  
  if (config.feishu.enabled) {
    totalCount++;
    watchdogManager.feedWatchdog();
    if (sendToFeishu(title, content)) successCount++;
  }
  
  if (config.custom.enabled) {
    totalCount++;
    watchdogManager.feedWatchdog();
    if (sendToCustom(title, content)) successCount++;
  }
  
  float successRate = totalCount > 0 ? (float)successCount / totalCount * 100 : 0;
  
  bool success = successCount > 0;
  
  if (success) {
    setStatusLED("ready");
    LOGI("PUSH", "push_success_rate",
         String(successCount).c_str(),
         String(totalCount).c_str(),
         String(successRate, 1).c_str());
    statisticsManager.incrementPushSuccess();
  } else {
    setStatusLED("error");
    LOGE("PUSH", "push_all_failed");
    statisticsManager.incrementPushFailed();
    if (!isRetry && smsId > 0) {
      retryManager.scheduleRetry(smsId, sender, content);
    } else if (!isRetry) {
      retryManager.scheduleRetry(0, sender, content);
    } else {
      LOGW("RETRY", "retry_still_failed");
    }
  }

  if (smsId > 0) {
    String attemptAt = getTimestampMsString();
    if (success) {
      retryManager.cancelRetry(smsId);
      const char* status = manual ? SMSStatus::MANUAL_FORWARD_SUCCESS : SMSStatus::FORWARD_SUCCESS;
      smsStorage.updateSMSStatus(smsId, status, attemptAt, "", -1);
    } else if (!isRetry) {
      smsStorage.updateSMSStatus(smsId, SMSStatus::RETRY_SCHEDULED, attemptAt, "push_all_failed", -1);
    }
  }
  
  return success;
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
