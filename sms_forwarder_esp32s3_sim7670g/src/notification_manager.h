#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "notification_protocol.h"
#include "config_manager.h"
#include "notification_types.h"

class NotificationManager {
public:
  static void init();
  static void processQueue();
  static void cancelSMS(int smsId);
  static void cancelAllSMS();
  static bool isSMSActive(int smsId);
  static bool startTest(const String& message, uint32_t& jobId);
  static bool getTestResult(uint32_t jobId, NotificationTestResult& result);
  static bool sendToBark(const String& title, const String& content, const Config& settings = config);
  static bool sendToServerChan(const String& title, const String& content, const Config& settings = config);
  static bool sendToTelegram(const String& title, const String& content, const Config& settings = config);
  static bool sendToDingTalk(const String& title, const String& content, const Config& settings = config);
  static bool sendToFeishu(const String& title, const String& content, const Config& settings = config);
  static bool sendToCustom(const String& title, const String& content, const Config& settings = config);
  
  static bool forwardSMS(const String& sender, const String& content, bool isRetry = false, int smsId = 0, bool manual = false,
                         NotificationKind kind = NotificationKind::Sms, int32_t reportDate = 0);
  
private:
  static bool ensureWorkerReady();
  static bool sendHTTPRequest(const String& url, const String& payload = "", const String& contentType = "application/x-www-form-urlencoded", NotificationProvider provider = NotificationProvider::Custom, const Config& settings = config);
  static String urlEncode(const String& str);
  static String createJsonPayload(const String& title, const String& content, NotificationProvider provider = NotificationProvider::Feishu);
};

extern NotificationManager notificationManager;

#endif
