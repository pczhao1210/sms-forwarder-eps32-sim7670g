const fs = require('node:fs');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'retry_manager.cpp'), 'utf8');
const methods = ['restoreRetriesFromStorage', 'scheduleRetry', 'processRetries', 'handleRetryResult', 'clearRetries', 'cancelRetry', 'getRetryCount'];
runCpp(`
#include "${path.join(root, 'retry_manager.h')}"
#include "${path.join(root, 'millis_utils.h')}"
#include <cassert>
#include <iostream>
#define LOGI(...) ((void)0)
#define LOGW(...) ((void)0)
#define LOGE(...) ((void)0)
enum {LOG_INFO, LOG_ERROR};
struct { void addLog(int, const char*, const String&) {} } logManager;
namespace SMSStatus {
const String RETRY_SCHEDULED = "retry_scheduled", RETRYING = "retrying", PENDING_FORWARD = "pending_forward";
const String RETRY_EXHAUSTED = "retry_exhausted", FORWARD_SUCCESS = "forward_success";
}
struct SMSRecord { int id; String sender; String content; String status; int retryCount; String lastError; };
struct {
  std::vector<SMSRecord> records;
  bool failPersist = false;
  int getSMSCount() { return records.size(); }
  bool getSMSAt(size_t index, SMSRecord& result) { result = records.at(index); return true; }
  bool updateSMSStatus(int id, const String& status, const String& = "", const String& error = "", int count = -1, bool persist = true) {
    if (persist && failPersist) return false;
    for (auto& record : records) if (record.id == id) {
      record.status = status;
      record.lastError = error;
      if (count >= 0) record.retryCount = count;
      return true;
    }
    return false;
  }
  bool flush() { return !failPersist; }
} smsStorage;
struct {
  int activeId = 0;
  bool accept = true;
  std::vector<int> calls;
  bool isSMSActive(int id) { return id == activeId; }
  bool forwardSMS(const String&, const String&, bool retry, int id, bool manual) {
    assert(retry && !manual);
    calls.push_back(id);
    return accept;
  }
} notificationManager;
struct {
  int retries = 0, forwards = 0;
  void incrementRetries() { retries++; }
  void incrementSMSForwarded() { forwards++; }
} statisticsManager;
uint32_t tick = 0;
uint32_t millis() { return tick; }
String getTimestampMsString() { return "10000"; }
${extractFunction(source, 'static bool shouldRestoreRetryStatus(')}
${methods.map(name => extractFunction(source, `${name === 'handleRetryResult' ? 'bool' : name === 'getRetryCount' ? 'int' : 'void'} RetryManager::${name}(`)).join('\n')}
int main() {
  RetryManager manager;
  smsStorage.records = {{1, "sender", "one", SMSStatus::RETRYING, 3, ""},
                        {2, "sender", "two", SMSStatus::PENDING_FORWARD, 0, ""},
                        {3, "sender", "three", SMSStatus::RETRY_SCHEDULED, 2, ""}};
  manager.restoreRetriesFromStorage();
  manager.restoreRetriesFromStorage();
  assert(manager.getRetryCount() == 2);
  assert(smsStorage.records[0].status == SMSStatus::RETRY_EXHAUSTED);
  notificationManager.activeId = 2;
  smsStorage.failPersist = true;
  tick = 10001;
  manager.processRetries();
  assert(notificationManager.calls.empty() && statisticsManager.retries == 0);
  smsStorage.failPersist = false;
  tick += 5001;
  manager.processRetries();
  assert(notificationManager.calls == std::vector<int>{3});
  assert(smsStorage.records[2].retryCount == 3 && statisticsManager.retries == 1);
  smsStorage.failPersist = true;
  assert(!manager.handleRetryResult(3, "sender", "three", false));
  tick += 5001;
  manager.processRetries();
  assert(notificationManager.calls.size() == 1);
  smsStorage.failPersist = false;
  assert(manager.handleRetryResult(3, "sender", "three", false));
  assert(manager.getRetryCount() == 1 && smsStorage.records[2].status == SMSStatus::RETRY_EXHAUSTED);
  notificationManager.activeId = 0;
  notificationManager.accept = false;
  tick += 5001;
  manager.processRetries();
  assert(smsStorage.records[1].retryCount == 0 && statisticsManager.retries == 1);
  notificationManager.accept = true;
  tick += 5001;
  manager.processRetries();
  assert(smsStorage.records[1].retryCount == 1 && statisticsManager.retries == 2);
  smsStorage.failPersist = true;
  assert(!manager.handleRetryResult(2, "sender", "two", true));
  assert(statisticsManager.forwards == 0 && manager.getRetryCount() == 1);
  smsStorage.failPersist = false;
  assert(manager.handleRetryResult(2, "sender", "two", true));
  assert(statisticsManager.forwards == 1 && manager.getRetryCount() == 0);
  manager.scheduleRetry(2, "sender", "two");
  manager.scheduleRetry(2, "sender", "two");
  assert(manager.getRetryCount() == 1);
  manager.cancelRetry(2);
  assert(manager.getRetryCount() == 0);
  std::cout << "Retry recovery, single admission and durable result tests passed.\\n";
}
`);