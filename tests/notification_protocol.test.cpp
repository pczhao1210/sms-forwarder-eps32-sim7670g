#include <cassert>
#include <iostream>
#include "../sms_forwarder_esp32s3_sim7670g/src/notification_protocol.h"

int main() {
  DynamicJsonDocument doc(2048);
  populateTextNotificationPayload(doc, NotificationProvider::DingTalk, "title", "message");
  assert(doc["msgtype"] == "text");
  assert(doc["text"]["content"] == "title\nmessage");
  assert(!doc.containsKey("msg_type"));
  doc.clear();
  populateTextNotificationPayload(doc, NotificationProvider::Feishu, "title", "message");
  assert(doc["msg_type"] == "text");
  assert(doc["content"]["text"] == "title\nmessage");
  assert(!doc.containsKey("msgtype"));

  struct Case { NotificationProvider provider; const char* body; bool success; };
  const Case cases[] = {
    {NotificationProvider::Bark, "{\"code\":200}", true},
    {NotificationProvider::Bark, "{\"code\":400}", false},
    {NotificationProvider::ServerChan, "{\"code\":0}", true},
    {NotificationProvider::ServerChan, "{\"code\":40001}", false},
    {NotificationProvider::Telegram, "{\"ok\":true}", true},
    {NotificationProvider::Telegram, "{\"ok\":false}", false},
    {NotificationProvider::Telegram, "{\"ok\":\"false\"}", false},
    {NotificationProvider::DingTalk, "{\"errcode\":0}", true},
    {NotificationProvider::DingTalk, "{\"errcode\":\"0\"}", true},
    {NotificationProvider::DingTalk, "{\"errcode\":310000}", false},
    {NotificationProvider::DingTalk, "{}", false},
    {NotificationProvider::Feishu, "{\"code\":0}", true},
    {NotificationProvider::Feishu, "{\"StatusCode\":0}", true},
    {NotificationProvider::Feishu, "{\"code\":19001}", false},
    {NotificationProvider::Custom, "{}", true},
  };
  for (const auto& entry : cases) {
    assert(!deserializeJson(doc, entry.body));
    assert(notificationResponseSucceeded(entry.provider, 200, doc.as<JsonVariantConst>()) == entry.success);
    assert(!notificationResponseSucceeded(entry.provider, 500, doc.as<JsonVariantConst>()));
  }
  std::cout << "Notification provider protocol tests passed.\n";
}