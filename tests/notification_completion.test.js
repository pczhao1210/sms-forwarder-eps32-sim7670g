const fs = require('node:fs');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'notification_manager.cpp'), 'utf8');
const structures = source.slice(source.indexOf('struct NotificationJob'), source.indexOf('std::deque<NotificationJob>'));
runCpp(`
#include "${path.join(root, 'config_manager.h')}"
#include "${path.join(root, 'notification_types.h')}"
#include "${path.join(root, 'millis_utils.h')}"
#include <cassert>
#include <memory>
#include <iostream>
#define LOGW(...) (void)0
${structures}
namespace SMSStatus {
 const char* MANUAL_FORWARD_SUCCESS = "manual_success";
 const char* FORWARD_SUCCESS = "success";
 const char* RETRY_SCHEDULED = "retry";
}
NotificationTestResult latestNotificationTest;
NotificationJob lastRetry;
int retryAdmissions = 0;
bool durable = false;
uint32_t millis() { return 10; }
String getTimestampMsString() { return "123"; }
bool enqueueNotificationJob(const NotificationJob& job) { lastRetry = job; retryAdmissions++; return true; }
struct Record { int id; };
struct {
 Record getSMSById(int id) { return {id}; }
 bool updateSMSStatus(int, const char*, const String&, const char*, int) { return durable; }
} smsStorage;
struct {
 int successes = 0;
 int failures = 0;
 int forwarded = 0;
 int32_t daily = 0;
 bool markDailyReportSent(int32_t date) { if (durable) daily = date; return durable; }
 bool markWeeklyReportSent(int32_t) { return durable; }
 void incrementSMSForwarded() { forwarded++; }
 void incrementPushSuccess() { successes++; }
 void incrementPushFailed() { failures++; }
} statisticsManager;
struct {
 bool handleRetryResult(int, const String&, const String&, bool) { return durable; }
 void scheduleRetry(int, const String&, const String&) {}
 void cancelRetry(int) {}
} retryManager;
const int LOG_WARN = 2;
struct { void addLog(int, const char*, const char*) {} } logManager;
struct { void noteRoamingAlertDelivered() {} } networkManager;
${extractFunction(source, 'static bool finalizeNotificationResult(const NotificationResult& result) {')}
int main() {
 NotificationResult sms;
 sms.job.smsId = 42;
 sms.success = true;
 assert(!finalizeNotificationResult(sms));
 assert(statisticsManager.forwarded == 0 && statisticsManager.successes == 0);
 durable = true;
 assert(finalizeNotificationResult(sms));
 assert(statisticsManager.forwarded == 1 && statisticsManager.successes == 1);
 NotificationResult report;
 report.job.kind = NotificationKind::DailyReport;
 report.job.reportDate = 20260905;
 assert(finalizeNotificationResult(report));
 assert(statisticsManager.daily == 0 && retryAdmissions == 1);
 assert(lastRetry.systemAttempts == 1 && lastRetry.delayed);
 report.success = true;
 durable = false;
 assert(!finalizeNotificationResult(report));
 assert(statisticsManager.daily == 0);
 durable = true;
 assert(finalizeNotificationResult(report));
 assert(statisticsManager.daily == 20260905);
 report.success = false;
 report.job.systemAttempts = 3;
 assert(finalizeNotificationResult(report));
 assert(retryAdmissions == 1);
 NotificationResult test;
 test.job.kind = NotificationKind::Test;
 test.job.testId = 7;
 test.enabledMask = 63;
 test.successMask = 8;
 assert(finalizeNotificationResult(test));
 assert(latestNotificationTest.complete && latestNotificationTest.id == 7);
 assert(latestNotificationTest.enabledMask == 63 && latestNotificationTest.successMask == 8);
 std::cout << "Notification completion and report tests passed.\\n";
}
`);