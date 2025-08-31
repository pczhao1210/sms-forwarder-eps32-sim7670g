#include "notification_manager.h"
#include "config_manager.h"
#include "log_manager.h"
#include "led_controller.h"
#include "statistics_manager.h"
#include "retry_manager.h"
#include "watchdog_manager.h"

NotificationManager notificationManager;

bool NotificationManager::sendToBark(const String& title, const String& content) {
  if (!config.bark.enabled || config.bark.key.isEmpty()) return false;
  
  String url = config.bark.url + "/" + config.bark.key + "/" + urlEncode(title) + "/" + urlEncode(content);
  logManager.addLog(LOG_INFO, "BARK", "URL: " + url);
  logManager.addLog(LOG_INFO, "BARK", "Content: " + content);
  
  bool success = sendHTTPRequest(url);
  logManager.addLog(success ? LOG_INFO : LOG_ERROR, "BARK", 
    String("发送") + (success ? "成功" : "失败"));
  return success;
}

bool NotificationManager::sendToServerChan(const String& title, const String& content) {
  if (!config.serverChan.enabled || config.serverChan.key.isEmpty()) return false;
  
  String url = config.serverChan.url + "/" + config.serverChan.key + ".send";
  String payload = "title=" + urlEncode(title) + "&desp=" + urlEncode(content);
  logManager.addLog(LOG_INFO, "SERVERCHAN", "URL: " + url);
  logManager.addLog(LOG_INFO, "SERVERCHAN", "Content: " + content);
  
  bool success = sendHTTPRequest(url, payload);
  logManager.addLog(success ? LOG_INFO : LOG_ERROR, "SERVERCHAN", 
    String("发送") + (success ? "成功" : "失败"));
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

void NotificationManager::forwardSMS(const String& sender, const String& content) {
  setStatusLED("working");
  logManager.addLog(LOG_INFO, "SMS", "收到来自 " + sender + " 的短信");
  
  String title = "短信转发 - " + sender;
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
  
  if (successCount > 0) {
    setStatusLED("ready");
    logManager.addLog(LOG_INFO, "PUSH", 
      "推送成功 " + String(successCount) + "/" + String(totalCount) + 
      " (" + String(successRate, 1) + "%)");
    statisticsManager.incrementSMSForwarded();
    statisticsManager.incrementPushSuccess();
  } else {
    setStatusLED("error");
    logManager.addLog(LOG_ERROR, "PUSH", "所有推送平台均失败");
    statisticsManager.incrementPushFailed();
    retryManager.scheduleRetry(sender, content);
  }
}

bool NotificationManager::sendHTTPRequest(const String& url, const String& payload, const String& contentType) {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  http.begin(client, url);
  http.addHeader("Content-Type", contentType);
  http.setTimeout(10000);
  
  int httpCode;
  if (payload.isEmpty()) {
    httpCode = http.GET();
  } else {
    httpCode = http.POST(payload);
  }
  
  bool success = (httpCode == 200);
  http.end();
  
  return success;
}

String NotificationManager::urlEncode(const String& str) {
  String encoded = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    unsigned char uc = (unsigned char)c;
    
    // 保留安全字符
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    }
    // 保留UTF-8中文字符（不编码）
    else if (uc >= 0x80) {
      encoded += c;
    }
    // 只编码需要编码的特殊字符
    else {
      encoded += "%";
      if (uc < 16) encoded += "0";
      encoded += String(uc, HEX);
    }
  }
  return encoded;
}

String NotificationManager::createJsonPayload(const String& title, const String& content) {
  return "{\"msg_type\":\"text\",\"content\":{\"text\":\"" + title + "\\n" + content + "\"}}";
}