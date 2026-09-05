const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { execFileSync } = require('node:child_process');

const source = fs.readFileSync(path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src/sms_handler.cpp'), 'utf8');
const signature = 'bool processSingleSMS(const String& sender, const String& content, int smsIndex) {';
const start = source.indexOf(signature);
assert.ok(start >= 0, 'production admission function exists');
const implementation = source.slice(start, source.indexOf('\n}', start) + 2);
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'sms-admission-'));
const executable = path.join(directory, 'test');
const harness = `
#include <cassert>
#include <string>
#include <vector>
struct String : std::string {
  using std::string::string;
  String(const std::string& value) : std::string(value) {}
  String(size_t value) : std::string(std::to_string(value)) {}
};
namespace SMSStatus {
  const char* PENDING_FORWARD = "pending_forward";
  const char* FILTERED = "filtered";
  const char* INVALID = "invalid";
}
bool valid = true;
bool allowed = true;
int scans = 0;
std::vector<int> deletions;
struct {
  int result = 42;
  String status;
  int saveSMS(const String&, const String&, const String&, const String& value) {
    status = value;
    return result;
  }
} smsStorage;
struct {
  int received = 0;
  int filtered = 0;
  void incrementSMSReceived() { received++; }
  void incrementSMSFiltered() { filtered++; }
  void updateLastSMS(const String&) {}
} statisticsManager;
struct { void updateActivity() {} } sleepManager;
struct { bool shouldForwardSMS(const String&, const String&) { return allowed; } } smsFilter;
struct {
  int calls = 0;
  int lastId = 0;
  bool forwardSMS(const String&, const String&, bool, int id, bool) {
    calls++;
    lastId = id;
    return false;
  }
} notificationManager;
struct { void addLog(int, const char*, const char*) {} } logManager;
const int LOG_ERROR = 3;
#define LOGI(...) (void)0
#define LOGW(...) (void)0
String getTimestampMsString() { return "12345"; }
String i18nFormat(const char* key) { return key; }
bool isValidSMSContent(const String&) { return valid; }
void requestSMSFullScan() { scans++; }
void deleteSMS(int index) { deletions.push_back(index); }
${implementation}
int main() {
  for (int scenario = 0; scenario < 3; scenario++) {
    valid = scenario != 1;
    allowed = scenario != 2;
    smsStorage.result = 0;
    assert(!processSingleSMS("sender", "message", 7));
    assert(deletions.empty());
    assert(notificationManager.calls == 0);
    assert(statisticsManager.received == 0);
  }
  assert(scans == 3);
  smsStorage.result = 42;
  valid = true;
  allowed = true;
  assert(processSingleSMS("sender", "message", 7));
  assert(notificationManager.calls == 1 && notificationManager.lastId == 42);
  assert(deletions == std::vector<int>{7});
  assert(statisticsManager.received == 1);
  allowed = false;
  assert(processSingleSMS("sender", "message", 8));
  assert(smsStorage.status == SMSStatus::FILTERED);
  assert(statisticsManager.filtered == 1 && notificationManager.calls == 1);
  valid = false;
  assert(processSingleSMS("sender", "message", 9));
  assert(smsStorage.status == SMSStatus::INVALID);
  assert((deletions == std::vector<int>{7, 8, 9}));
}
`;

try {
  const compiler = process.env.CXX || 'c++';
  const args = (process.env.CXX_ARGS || '').split(/\s+/).filter(Boolean);
  execFileSync(compiler, [...args, '-std=c++17', '-Wall', '-Wextra', '-x', 'c++', '-', '-o', executable], { input: harness, stdio: ['pipe', 'inherit', 'inherit'] });
  execFileSync(executable, { stdio: 'inherit' });
  console.log('SMS admission tests passed (production C++).');
} finally {
  fs.rmSync(directory, { recursive: true, force: true });
}