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
runCpp(`
#include <Arduino.h>
#include <cassert>
#include <iostream>
#include "${path.join(root, 'millis_utils.h')}"
#include "${path.join(root, 'at_response.h')}"
#include "${path.join(root, 'input_validation.h')}"
#include "${path.join(root, 'pdp_auth.h')}"
#define LOGI(...) ((void)0)
#define LOGD(...) ((void)0)
#define LOGW(...) ((void)0)
#define LOGE(...) ((void)0)
enum {LOG_DEBUG, LOG_WARN, LOG_ERROR};
enum {SIM_STATE_READY, SIM_STATE_WAIT_AT_OK, SIM_STATE_INIT_CMDS, SIM_STATE_CONFIG_APN};
enum {DATA_POLICY_ALWAYS_OFF};
struct { void addLog(int, const char*, const String&) {} } logManager;
struct { void println(const String&) {} } Serial;
struct {
  struct { bool atCommandEcho = false; } debug;
  struct { int dataPolicy = DATA_POLICY_ALWAYS_OFF; } network;
} config;
uint32_t tick = 0;
uint32_t millis() { return tick; }
void delay(uint32_t duration) { tick += duration; }
struct { void feedWatchdog() {} } watchdogManager;
struct {
  std::string input;
  std::vector<String> commands;
  int available() { return input.size(); }
  int read() { char value = input[0]; input.erase(0, 1); return value; }
  void println(const String& command) { commands.push_back(command); }
  void flush() {}
} sim7670g;
int simState = SIM_STATE_READY;
bool waitingForSMSRead = false, waitingForSMSDeleteResponse = false;
bool waitingForResponse = false, manualATInProgress = false, smsSending = false, waitingForSMSStorageCount = false;
bool manualCMGRMode = false, manualCMGLMode = false, manualCMGLReceiving = false, cmglReceiving = false;
bool pendingSMSProcessing = false, awaitingCmtPdu = false, smsDeleteBackoff = false, simResetRequested = false;
bool modemAsyncWorkerActive = false;
uint32_t firstSMSTime = 0, cmtPduStartedMs = 0, smsDeleteRetryAt = 0, cmglStartTime = 0, manualCMGLStartTime = 0;
int currentSMSIndex = 1, currentSMSDeleteIndex = -1, expectedPDULenChars = 0;
int foundSMSCount = 0, currentCMGRIndex = 0, totalSMSCount = 0, maxSMSIndex = 50;
constexpr int MAX_PENDING_SMS_INDEXES = 50, MAX_SMS_BUFFER_SIZE = 4096, MAX_PENDING_SMS_DELETES = 256;
int pendingSMSCount = 0, pendingSMSIndexes[MAX_PENDING_SMS_INDEXES] = {};
int pendingSMSDeleteCount = 0, pendingSMSDeleteIndexes[MAX_PENDING_SMS_DELETES] = {};
uint32_t smsReadStartedMs = 0, smsDeleteStartedMs = 0;
int cmdRetryCount = 0, atRetryCount = 0, initCmdIndex = 0, INIT_CMD_COUNT = 0;
const char* initCmds[] = {"AT"};
bool pdpApnConfigured = false, pdpAuthConfigured = false;
void changeState(int state) { simState = state; }
String currentNetworkConfigCommand() { return ""; }
bool sendNextNetworkConfigCommand() { return false; }
bool resendCurrentNetworkConfigCommand() { return false; }
void sendNetworkConfig() {}
void sendAT(const char* command) { waitingForResponse = true; sim7670g.println(command); }
String smsReadBuffer, manualCMGLBuffer;
int scans = 0, accepted = 0, lists = 0, batches = 0, clears = 0;
std::vector<int> acceptedIndexes;
String listResponse;
void requestSMSFullScan() { scans++; }
void requestPendingSMSFullScan(int) { scans++; }
void queueSMSDelete(int index);
void readSMSByIndex(int index);
bool validatePduLength(const String&, int) { return true; }
void storePendingCMTSMS(const String&) {}
void storeTempSMSFromCMGR(const String&, int index) { accepted++; acceptedIndexes.push_back(index); }
void handleRawSMSData(const String&, int index) { accepted++; acceptedIndexes.push_back(index); }
void processBatchedSMS() { batches++; }
void clearTempSMSStorage() { clears++; }
void processCMGLResponse(const String& response) { lists++; listResponse = response; }
${extractFunction(source, 'static bool isLikelyPduPayloadLine(')}
${extractFunction(source, 'static bool processSmsUrc(')}
${extractFunction(source, 'static void finishSMSListRead(')}
${extractFunction(source, 'static void finishSMSRead(')}
${extractFunction(source, 'static bool hasActiveModemTransaction()')}
${extractFunction(source, 'static bool isModemBusyForStatus()')}
${extractFunction(source, 'bool isModemAvailableForMaintenance()')}
${extractFunction(source, 'static bool waitForSmsExpected(const char* expected, unsigned long timeoutMs, String& responseOut) {')}
${extractFunction(source, 'void readSMSByIndex(int index) {')}
${extractFunction(source, 'static bool readNextPendingSMS() {')}
${extractFunction(source, 'void queueSMSDelete(int index) {')}
${extractFunction(source, 'static bool sendNextSMSDelete() {')}
${extractFunction(source, 'void checkAllSMS() {')}
${processor}
void finishMessage() {
  processLine("+CMGR: 0,,20");
  processLine("00112233445566778899001122334455");
  processLine("OK");
}
int main() {
  assert(isModemAvailableForMaintenance());
  for (bool* owner : {&waitingForResponse, &waitingForSMSRead, &waitingForSMSDeleteResponse,
       &manualATInProgress, &smsSending, &manualCMGRMode, &manualCMGLMode, &awaitingCmtPdu,
       &waitingForSMSStorageCount, &simResetRequested, &modemAsyncWorkerActive}) {
    *owner = true;
    assert(!isModemAvailableForMaintenance());
    *owner = false;
  }
  simState = SIM_STATE_INIT_CMDS;
  assert(!isModemAvailableForMaintenance());
  simState = SIM_STATE_READY;
  processLine("+CMTI: \\"SM\\",0");
  assert(pendingSMSProcessing && pendingSMSCount == 1 && pendingSMSIndexes[0] == 0);
  processLine("+CMTI: \\"SM\\",0");
  assert(pendingSMSCount == 1);
  for (const char* invalid : {"+CMTI: \\"SM\\"", "+CMTI: \\"SM\\",", "+CMTI: \\"SM\\",bad",
       "+CMTI: \\"SM\\",-1", "+CMTI: \\"SM\\",0x", "+CMTI: \\"SM\\",2147483648"}) {
    processLine(invalid);
    assert(pendingSMSCount == 1);
  }
  pendingSMSCount = 0;
  pendingSMSProcessing = false;
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
  assert(!waitingForSMSDeleteResponse && pendingSMSDeleteIndexes[0] == 12 && smsDeleteBackoff);
  pendingSMSDeleteCount = 0;
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
  pendingSMSCount = 0;
  pendingSMSProcessing = false;
  processLine("+CMTI: \\"SM\\", 0");
  assert(pendingSMSCount == 1);
  pendingSMSProcessing = false;
  assert(readNextPendingSMS());
  assert(currentSMSIndex == 0 && waitingForSMSRead && pendingSMSCount == 0);
  assert(sim7670g.commands.back() == "AT+CMGR=0");
  finishMessage();
  assert(!waitingForSMSRead && acceptedIndexes.back() == 0);

  smsDeleteBackoff = false;
  queueSMSDelete(-1);
  queueSMSDelete(0);
  queueSMSDelete(0);
  assert(pendingSMSDeleteCount == 1);
  assert(sendNextSMSDelete() && sim7670g.commands.back() == "AT+CMGD=0");
  queueSMSDelete(0);
  assert(pendingSMSDeleteCount == 0);
  processLine("+CMS ERROR: 500");
  assert(currentSMSDeleteIndex == -1 && pendingSMSDeleteCount == 1 && smsDeleteBackoff);
  assert(pendingSMSDeleteIndexes[0] == 0 && !sendNextSMSDelete());
  tick += 10000;
  assert(sendNextSMSDelete() && sim7670g.commands.back() == "AT+CMGD=0");
  processLine("OK");
  assert(!waitingForSMSDeleteResponse && pendingSMSDeleteCount == 0);
  queueSMSDelete(0);
  assert(sendNextSMSDelete());
  processLine("OK");

  checkAllSMS();
  assert(waitingForSMSStorageCount && sim7670g.commands.back() == "AT+CPMS?");
  processLine("+CPMS: \\"SM\\",1,10,\\"SM\\",1,10,\\"SM\\",1,10");
  assert(totalSMSCount == 1 && maxSMSIndex == 10 && !waitingForSMSRead);
  processLine("OK");
  assert(!waitingForResponse && !waitingForSMSStorageCount && manualCMGRMode && clears == 1);
  assert(currentSMSIndex == 0 && sim7670g.commands.back() == "AT+CMGR=0");
  size_t commandCount = sim7670g.commands.size();
  finishMessage();
  assert(!waitingForSMSRead && !manualCMGRMode && acceptedIndexes.back() == 0 && batches == 1);
  assert(sim7670g.commands.size() == commandCount);

  // Sparse zero-based and one-based stores must both retain their last slot.
  for (int lastSlot : {9, 10}) {
    checkAllSMS();
    processLine("+CPMS: \\"SM\\",1,10,\\"SM\\",1,10,\\"SM\\",1,10");
    processLine("OK");
    for (int slot = 0; slot < lastSlot; slot++) {
      assert(waitingForSMSRead && currentSMSIndex == slot && manualCMGRMode);
      processLine(slot == 0 ? "+CMS ERROR: invalid memory index" : "OK");
    }
    assert(currentSMSIndex == lastSlot);
    commandCount = sim7670g.commands.size();
    finishMessage();
    assert(!waitingForSMSRead && !manualCMGRMode && acceptedIndexes.back() == lastSlot);
    assert(sim7670g.commands.size() == commandCount);
  }
  checkAllSMS();
  processLine("+CPMS: \\"SM\\",1,10,\\"SM\\",1,10,\\"SM\\",1,10");
  processLine("OK");
  for (int slot = 0; slot <= 10; slot++) {
    assert(currentSMSIndex == slot && waitingForSMSRead);
    processLine("+CMS ERROR: invalid memory index");
  }
  assert(!waitingForSMSRead && !manualCMGRMode);
  checkAllSMS();
  processLine("+CPMS: \\"SM\\",0,10,\\"SM\\",0,10,\\"SM\\",0,10");
  commandCount = sim7670g.commands.size();
  processLine("OK");
  assert(!waitingForSMSRead && !manualCMGRMode && sim7670g.commands.size() == commandCount);
  processLine("+CMGL:0,0,,20");
  assert(waitingForSMSRead && currentSMSIndex == 0);
  finishMessage();
  commandCount = sim7670g.commands.size();
  processLine("+CMGL: bad,0,,20");
  assert(!waitingForSMSRead && sim7670g.commands.size() == commandCount);
  std::cout << "SMS AT response ownership and terminal-state tests passed.\\n";
}
`);