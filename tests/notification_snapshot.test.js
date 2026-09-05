const fs = require('node:fs');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'notification_manager.cpp'), 'utf8');
const structures = source.slice(source.indexOf('struct NotificationJob'), source.indexOf('std::deque<NotificationJob>'));
runCpp(`
#include "${path.join(root, 'config_manager.h')}"
#include "${path.join(root, 'notification_types.h')}"
#include <cassert>
#include <memory>
#include <iostream>
#define LOGI(...) (void)0
${structures}
Config config{};
String usedKey;
struct { void feedWatchdog() {} } watchdogManager;
bool isNotificationJobCanceled(int) { return false; }
struct NotificationManager {
  static bool sendToBark(const String&, const String&, const Config& settings) { usedKey = settings.bark.key; return true; }
  static bool sendToServerChan(const String&, const String&, const Config&) { return false; }
  static bool sendToTelegram(const String&, const String&, const Config&) { return false; }
  static bool sendToDingTalk(const String&, const String&, const Config&) { return false; }
  static bool sendToFeishu(const String&, const String&, const Config&) { return false; }
  static bool sendToCustom(const String&, const String&, const Config&) { return false; }
};
${extractFunction(source, 'static NotificationResult executeNotificationJob(const NotificationJob& job) {')}
int main() {
  config.bark.enabled = true;
  config.bark.key = "old-key";
  NotificationJob job;
  job.title = "title";
  job.settings = std::make_shared<const Config>(config);
  config.bark.enabled = false;
  config.bark.key = "new-key";
  config.telegram.enabled = true;
  auto result = executeNotificationJob(job);
  assert(usedKey == "old-key");
  assert(result.totalCount == 1 && result.successCount == 1 && result.success);
  std::cout << "Notification configuration snapshot test passed.\\n";
}
`);