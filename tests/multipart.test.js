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
  'void handleLongSMSFragment(const String& sender, const String& rawContent, int smsIndex) {',
  'void handleCMTPDU(const String& pduHex) {',
  'void handleCMTSMS(const String& cmtData) {',
  'TempSMSData parseTempSMSLine(const String& line) {',
  'void processSingleCMGLEntry(const String& entry) {',
];

runCpp(`
#include "SPIFFS.h"
#include <cassert>
#include <iostream>
#include "${path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src/input_validation.h')}"
${structures}
${extractFunction(source, 'struct CMTData')};
${source.match(/^static const int NO_SIM_SMS_INDEX = .*;$/m)[0]}
#define LOGI(...) (void)0
#define LOGW(...) (void)0
#define LOGD(...) (void)0
std::map<String, std::map<int, std::map<int, LongSMSFragment>>> longSMSBuffer;
std::map<String, PDUInfo> fixtures;
bool durable = true;
int deliveries = 0;
int scans = 0;
int deliveredIndex = -2;
String delivered;
std::vector<int> deleted;
std::vector<TempSMSData> stored;
unsigned long millis() { return 1; }
void requestSMSFullScan() { scans++; }
PDUInfo parsePDU(const String& value) { return fixtures[value]; }
String decodeUnicodeContent(const String& value) { return value; }
String decodeUCS2BE(const String& value) { return value; }
String decode8Bit(const String& value, int) { return value; }
String decode7BitWithOffset(const String& value, int, int) { return value; }
CMTData parseCMTData(const String& value) { return {0, value}; }
void readMoreLongSMSFragments(int) {}
String extractSender(const String&) { return "sender"; }
String extractRawContent(const String&) { return "pdu"; }
void storeTempSMS(const String& sender, const String& content, int index) {
  stored.push_back({sender, content, index});
}
void deleteSMS(int index) { deleted.push_back(index); }
bool processSingleSMS(const String&, const String& content, int index) {
  if (!durable) return false;
  deliveries++;
  delivered = content;
  deliveredIndex = index;
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
  assert(deliveredIndex == NO_SIM_SMS_INDEX);
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
  longSMSBuffer.clear();
  deleted.clear();
  for (auto& fixture : fixtures) fixture.second.sender = "sender";
  fixtures["short"] = fixtures["first"];
  fixtures["short"].hasUDH = false;
  fixtures["short"].userData = "direct";
  handleCMTPDU("short");
  assert(delivered == "direct" && deliveredIndex == NO_SIM_SMS_INDEX && deleted.empty());
  handleCMTSMS("short");
  assert(delivered == "direct" && deliveredIndex == NO_SIM_SMS_INDEX && deleted.empty());
  handleCMTPDU("first");
  assert(longSMSBuffer["sender"][7][1].smsIndexes.empty());
  storeLongSMSFragment("sender", parseLongSMSInfo("first"), "first", 0);
  storeLongSMSFragment("sender", parseLongSMSInfo("first"), "first", 0);
  assert((longSMSBuffer["sender"][7][1].smsIndexes == std::vector<int>{0}));
  handleCMTSMS("second");
  assert(delivered == "firstsecond" && deliveredIndex == NO_SIM_SMS_INDEX);
  assert(deleted == std::vector<int>{0} && longSMSBuffer.empty());
  deleted.clear();
  batch[0].smsIndex = 0;
  processCompleteLongSMSGroup("sender", 7, batch);
  assert((longSMSBuffer["sender"][7][1].smsIndexes == std::vector<int>{0}));
  handleLongSMSFragment("sender", "second", 1);
  assert((deleted == std::vector<int>{0, 1}));
  assert(longSMSBuffer.empty());
  auto parsed = parseTempSMSLine("sender|pdu|0");
  assert(parsed.sender == "sender" && parsed.rawContent == "pdu" && parsed.smsIndex == 0);
  for (const char* invalid : {"", "-1", "bad", "0x", "2147483648"}) {
    assert(parseTempSMSLine(String("sender|pdu|") + invalid).sender.isEmpty());
  }
  processSingleCMGLEntry("+CMGL: 0,0,,20\\npdu\\n");
  processSingleCMGLEntry("+CMGL:0,0,,20\\npdu\\n");
  assert(stored.size() == 2 && stored[0].smsIndex == 0 && stored[1].smsIndex == 0);
  for (const char* invalid : {"", "-1", "bad", "0x", "2147483648"}) {
    processSingleCMGLEntry(String("+CMGL: ") + invalid + ",0,,20\\npdu\\n");
  }
  assert(stored.size() == 2);
  std::cout << "Multipart receive tests passed (production C++).\\n";
}
`);