#ifndef NOTIFICATION_PROTOCOL_H
#define NOTIFICATION_PROTOCOL_H

#include <ArduinoJson.h>

enum class NotificationProvider { Bark, ServerChan, Telegram, DingTalk, Feishu, Custom };

inline bool notificationCodeEquals(JsonVariantConst value, int expected) {
  if (value.is<int>()) return value.as<int>() == expected;
  if (expected == 0 && value.is<const char*>()) return strcmp(value.as<const char*>(), "0") == 0;
  return false;
}

inline bool notificationResponseSucceeded(NotificationProvider provider, int status, JsonVariantConst body) {
  if (status < 200 || status >= 300) return false;
  switch (provider) {
    case NotificationProvider::Bark:
      return notificationCodeEquals(body["code"], 200);
    case NotificationProvider::ServerChan:
      return notificationCodeEquals(body["code"], 0) || notificationCodeEquals(body["errno"], 0);
    case NotificationProvider::Telegram:
      return body["ok"].is<bool>() && body["ok"].as<bool>();
    case NotificationProvider::DingTalk:
      return notificationCodeEquals(body["errcode"], 0);
    case NotificationProvider::Feishu:
      return notificationCodeEquals(body["code"], 0) || notificationCodeEquals(body["StatusCode"], 0);
    case NotificationProvider::Custom:
      return true;
  }
  return false;
}

inline void populateTextNotificationPayload(JsonDocument& doc, NotificationProvider provider,
                                            const String& title, const String& content) {
  if (provider == NotificationProvider::DingTalk) {
    doc["msgtype"] = "text";
    doc["text"]["content"] = title + "\n" + content;
  } else {
    doc["msg_type"] = "text";
    doc["content"]["text"] = title + "\n" + content;
  }
}

#endif