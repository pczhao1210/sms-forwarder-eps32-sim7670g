#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

class NotificationManager {
public:
  static bool sendToBark(const String& title, const String& content);
  static bool sendToServerChan(const String& title, const String& content);
  static bool sendToTelegram(const String& title, const String& content);
  static bool sendToDingTalk(const String& title, const String& content);
  static bool sendToFeishu(const String& title, const String& content);
  static bool sendToCustom(const String& title, const String& content);
  
  static bool forwardSMS(const String& sender, const String& content, bool isRetry = false);
  
private:
  static bool sendHTTPRequest(const String& url, const String& payload = "", const String& contentType = "application/x-www-form-urlencoded");
  static String urlEncode(const String& str);
  static String createJsonPayload(const String& title, const String& content);
};

extern NotificationManager notificationManager;

#endif
