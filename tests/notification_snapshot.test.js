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
#include <deque>
#include <iostream>
#define LOGI(...) (void)0
${structures}
Config config{};
String usedKey;
uint16_t usedHandshakeSeconds = 0;
int notificationQueueMutex = 0;
bool notificationWorkerBusy = false;
std::deque<NotificationJob> pendingNotificationJobs;
constexpr int portMAX_DELAY = 0;
constexpr int pdTRUE = 1;
bool lockFails = false;
int xSemaphoreTake(int, int) { return lockFails ? 0 : pdTRUE; }
void xSemaphoreGive(int) {}
struct { void feedWatchdog() {} } watchdogManager;
bool isNotificationJobCanceled(int) { return false; }
struct NotificationManager {
  static bool hasPendingWork();
  static bool sendToBark(const String&, const String&, const Config& settings) {
    usedKey = settings.bark.key;
    usedHandshakeSeconds = settings.tls.handshakeTimeoutSeconds;
    return true;
  }
  static bool sendToServerChan(const String&, const String&, const Config&) { return false; }
  static bool sendToTelegram(const String&, const String&, const Config&) { return false; }
  static bool sendToDingTalk(const String&, const String&, const Config&) { return false; }
  static bool sendToFeishu(const String&, const String&, const Config&) { return false; }
  static bool sendToCustom(const String&, const String&, const Config&) { return false; }
};
${extractFunction(source, 'static NotificationResult executeNotificationJob(const NotificationJob& job) {')}
${extractFunction(source, 'bool NotificationManager::hasPendingWork()')}
int main() {
  config.bark.enabled = true;
  config.bark.key = "old-key";
  config.tls.handshakeTimeoutSeconds = 20;
  NotificationJob job;
  job.title = "title";
  job.settings = std::make_shared<const Config>(config);
  config.bark.enabled = false;
  config.bark.key = "new-key";
  config.tls.handshakeTimeoutSeconds = 1;
  config.telegram.enabled = true;
  auto result = executeNotificationJob(job);
  assert(usedKey == "old-key");
  assert(usedHandshakeSeconds == 20);
  assert(result.totalCount == 1 && result.successCount == 1 && result.success);
  assert(!NotificationManager::hasPendingWork());
  notificationQueueMutex = 1;
  assert(!NotificationManager::hasPendingWork());
  pendingNotificationJobs.push_back(job);
  assert(NotificationManager::hasPendingWork());
  pendingNotificationJobs.clear();
  notificationWorkerBusy = true;
  assert(NotificationManager::hasPendingWork());
  notificationWorkerBusy = false;
  assert(!NotificationManager::hasPendingWork());
  lockFails = true;
  assert(NotificationManager::hasPendingWork());
  std::cout << "Notification configuration snapshot test passed.\\n";
}
`);