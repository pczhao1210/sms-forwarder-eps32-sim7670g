const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const { extractFunction, runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'sim7670g_manager.cpp'), 'utf8');
const configLogs = source.split('\n').filter(line => line.includes('"net_cfg_cmd_'));
assert.equal(configLogs.length, 5);
for (const line of configLogs) assert.ok(line.includes('redactPdpCredentials(command.c_str())'), line);
const processor = extractFunction(source, 'void processLine(String line)');
const boundary = processor.indexOf('  if (line.startsWith("+CMGL:") && !waitingForSMSRead && !manualCMGLMode)');
if (boundary < 0) throw new Error('Missing end of SMS-owned response handling');
runCpp(`
#include <Arduino.h>
#include <cassert>
#include <iostream>
#include "${path.join(root, 'millis_utils.h')}"
#include "${path.join(root, 'at_response.h')}"
#include "${path.join(root, 'pdp_auth.h')}"
#define LOGI(...) ((void)0)
#define LOGD(...) ((void)0)
#define LOGW(...) ((void)0)
#define LOGE(...) ((void)0)
enum {LOG_DEBUG, LOG_WARN, SIM_STATE_READY};
struct { void addLog(int, const char*, const String&) {} } logManager;
uint32_t tick = 0;
uint32_t millis() { return tick; }
void delay(uint32_t duration) { tick += duration; }
struct { void feedWatchdog() {} } watchdogManager;
struct {
  std::string input;
  int available() { return input.size(); }
  int read() { char value = input[0]; input.erase(0, 1); return value; }
} sim7670g;
int simState = SIM_STATE_READY;
bool waitingForSMSRead = false, waitingForSMSDeleteResponse = false;
bool waitingForResponse = false, manualATInProgress = false, smsSending = false, waitingForSMSStorageCount = false;
bool manualCMGRMode = false, manualCMGLMode = false, manualCMGLReceiving = false, cmglReceiving = false;
bool pendingSMSProcessing = false, awaitingCmtPdu = false, smsDeleteBackoff = false, simResetRequested = false;
uint32_t firstSMSTime = 0, cmtPduStartedMs = 0, smsDeleteRetryAt = 0, cmglStartTime = 0, manualCMGLStartTime = 0;
int currentSMSIndex = 1, currentSMSDeleteIndex = 0, expectedPDULenChars = 0;
int foundSMSCount = 0, currentCMGRIndex = 1, totalSMSCount = 0, maxSMSIndex = 50;
constexpr int MAX_PENDING_SMS_INDEXES = 50, MAX_SMS_BUFFER_SIZE = 4096;
int pendingSMSCount = 0, pendingSMSIndexes[MAX_PENDING_SMS_INDEXES] = {};
String smsReadBuffer, manualCMGLBuffer;
int scans = 0, accepted = 0, lists = 0, nextRead = 0;
String listResponse;
std::vector<int> deletes;
void requestSMSFullScan() { scans++; }
void requestPendingSMSFullScan(int) { scans++; }
void queueSMSDelete(int index) { deletes.push_back(index); }
bool validatePduLength(const String&, int) { return true; }
void storePendingCMTSMS(const String&) {}
void storeTempSMSFromCMGR(const String&, int) { accepted++; }
void handleRawSMSData(const String&, int) { accepted++; }
void processBatchedSMS() {}
void readSMSByIndex(int index) { nextRead = index; }
void processCMGLResponse(const String& response) { lists++; listResponse = response; }
${extractFunction(source, 'static bool isLikelyPduPayloadLine(')}
${extractFunction(source, 'static bool processSmsUrc(')}
${extractFunction(source, 'static void finishSMSListRead(')}
${extractFunction(source, 'static void finishSMSRead(')}
${extractFunction(source, 'static bool hasActiveModemTransaction()')}
${extractFunction(source, 'static bool isModemBusyForStatus()')}
${extractFunction(source, 'static bool waitForSmsExpected(const char* expected, unsigned long timeoutMs, String& responseOut) {')}
${processor.slice(0, boundary)}
}
int main() {
  processLine("+CMTI: \\"SM\\",8");
  assert(pendingSMSProcessing && pendingSMSCount == 1);
  waitingForSMSRead = true;
  processLine("+CMGR: 0,,20");
  String before = smsReadBuffer;
  processLine("+CMTI: \\"SM\\",9");
  assert(smsReadBuffer == before && pendingSMSCount == 2);
  processLine("+CMS ERROR: 321");
  assert(!waitingForSMSRead && smsReadBuffer.isEmpty() && accepted == 0 && scans > 0);
  waitingForSMSDeleteResponse = true;
  currentSMSDeleteIndex = 12;
  processLine("+CME ERROR: 10");
  assert(!waitingForSMSDeleteResponse && deletes.back() == 12 && smsDeleteBackoff);
  currentSMSIndex = -1;
  waitingForSMSRead = true;
  processLine("+CMGL: 1,0,,20");
  processLine("00112233445566778899001122334455");
  assert(cmglReceiving);
  processLine("OK");
  assert(!waitingForSMSRead && !cmglReceiving && lists == 1);
  assert(listResponse.indexOf("001122") >= 0);
  manualCMGLMode = true;
  processLine("+CMGL: 1,0,,20");
  processLine("+CMS ERROR: 500");
  assert(!manualCMGLMode && !manualCMGLReceiving && lists == 1 && manualCMGLBuffer.isEmpty());
  manualCMGLMode = true;
  processLine("+CMGL: 1,0,,20");
  processLine("OK");
  assert(!manualCMGLMode && !manualCMGLReceiving && lists == 2);
  processLine("+CMT: ,20");
  assert(awaitingCmtPdu);
  currentSMSIndex = 7;
  waitingForSMSRead = true;
  processLine("+CMGR: 0,,20");
  assert(!awaitingCmtPdu);
  processLine("00112233445566778899001122334455");
  processLine("OK");
  assert(accepted == 1 && !waitingForSMSRead);
  String response;
  sim7670g.input = "\\r\\n+CMTI: \\"SM\\",22\\r\\n+CMGS: 17\\r\\nOK\\r\\n";
  assert(waitForSmsExpected("+CMGS:", 50, response));
  assert(response.indexOf("+CMGS: 17") >= 0 && response.indexOf("OK") >= 0 && response.indexOf("CMTI") < 0);
  sim7670g.input = "\\r\\n+CMGS: 18\\r\\n";
  tick = UINT32_MAX - 10;
  assert(!waitForSmsExpected("+CMGS:", 50, response) && simResetRequested && isModemBusyForStatus());
  simResetRequested = false;
  sim7670g.input = "\\r\\n+CMS ERROR: 500\\r\\n";
  assert(!waitForSmsExpected("OK", 50, response) && !simResetRequested);
  sim7670g.input = "\\r\\n> ";
  assert(waitForSmsExpected(">", 50, response));
  std::cout << "SMS AT response ownership and terminal-state tests passed.\\n";
}
`);