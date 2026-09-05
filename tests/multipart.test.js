const fs = require('node:fs');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const source = fs.readFileSync(path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src/sms_handler.cpp'), 'utf8');
const structures = source.slice(source.indexOf('struct LongSMSInfo'), source.indexOf('struct CMTData'));
const signatures = [
  'bool isLongSMS(const String& pduData) {',
  'LongSMSInfo parseLongSMSInfo(const String& pduData) {',
  'void storeLongSMSFragment(const String& sender, const LongSMSInfo& info, const String& rawContent, int smsIndex) {',
  'bool needMoreFragments(const String& sender, int refNum, int totalParts) {',
  'void assembleAndProcessLongSMS(const String& sender, int refNum) {',
  'void processCompleteLongSMSGroup(const String& sender, int refNum, std::vector<TempSMSData>& fragments) {',
];

runCpp(`
#include "SPIFFS.h"
#include <cassert>
#include <iostream>
${structures}
#define LOGI(...) (void)0
#define LOGW(...) (void)0
std::map<String, std::map<int, std::map<int, LongSMSFragment>>> longSMSBuffer;
std::map<String, PDUInfo> fixtures;
bool durable = true;
int deliveries = 0;
int scans = 0;
String delivered;
std::vector<int> deleted;
unsigned long millis() { return 1; }
void requestSMSFullScan() { scans++; }
PDUInfo parsePDU(const String& value) { return fixtures[value]; }
String decodeUnicodeContent(const String& value) { return value; }
void deleteSMS(int index) { deleted.push_back(index); }
bool processSingleSMS(const String&, const String& content, int) {
  if (!durable) return false;
  deliveries++;
  delivered = content;
  return true;
}
${signatures.map(signature => extractFunction(source, signature)).join('\n')}
int main() {
  PDUInfo part{};
  part.valid = true;
  part.hasUDH = true;
  part.ref = 7;
  part.total = 2;
  part.seq = 1;
  fixtures["first"] = part;
  part.seq = 2;
  fixtures["second"] = part;
  std::vector<TempSMSData> batch{{"sender", "first", 3}};
  processCompleteLongSMSGroup("sender", 7, batch);
  assert(deliveries == 0 && deleted.empty());
  auto second = parseLongSMSInfo("second");
  storeLongSMSFragment("sender", second, "second", 4);
  durable = false;
  assembleAndProcessLongSMS("sender", 7);
  assert(deliveries == 0 && deleted.empty());
  durable = true;
  assembleAndProcessLongSMS("sender", 7);
  assert(deliveries == 1 && delivered == "firstsecond");
  assert((deleted == std::vector<int>{3, 4}));
  assert(longSMSBuffer.empty());
  assert(!isLongSMS("050003AB0201"));
  processCompleteLongSMSGroup("sender", 7, batch);
  part.total = 3;
  fixtures["conflict"] = part;
  storeLongSMSFragment("sender", parseLongSMSInfo("conflict"), "conflict", 5);
  storeLongSMSFragment("sender", second, "second", 4);
  assembleAndProcessLongSMS("sender", 7);
  assert(deliveries == 1 && scans > 0);
  std::cout << "Multipart receive tests passed (production C++).\\n";
}
`);