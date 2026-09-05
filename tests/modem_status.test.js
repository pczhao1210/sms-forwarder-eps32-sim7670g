const fs = require('node:fs');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'sim7670g_manager.cpp'), 'utf8');
const header = fs.readFileSync(path.join(root, 'sim7670g_manager.h'), 'utf8');
const types = header.slice(header.indexOf('struct SystemStatus {'), header.indexOf('extern SystemStatusManager'));
runCpp(`
#include <Arduino.h>
#include <cassert>
#include <iostream>
#include <map>
#include "${path.join(root, 'at_response.h')}"
#define LOGD(...) ((void)0)
#define LOGI(...) ((void)0)
enum { SIM_STATE_IDLE, SIM_STATE_READY };
int simState = SIM_STATE_IDLE;
bool busy = false;
uint32_t millis() { return 100; }
bool isModemBusyForStatus() { return busy; }
struct { struct { bool atCommandEcho = false; } debug; } config;
String mapOperatorName(const String& value) { return value; }
std::map<std::string, String> responses;
std::vector<std::string> commands;
String sendATCommand(const String& command) {
  assert(!busy);
  commands.push_back(command.c_str());
  return responses[command.c_str()];
}
${types}
SystemStatus SystemStatusManager::status;
${extractFunction(source, 'static String extractDigits(')}
${extractFunction(source, 'static String extractImsiFromResponse(')}
${extractFunction(source, 'static String extractHomeOperatorCodeFromImsi(')}
${extractFunction(source, 'static bool parseRegStatFromResponse(')}
${extractFunction(source, 'static bool queryStatusResponse(')}
${extractFunction(source, 'void SystemStatusManager::initStatus()')}
${extractFunction(source, 'SystemStatus SystemStatusManager::getStatus()')}
${extractFunction(source, 'void SystemStatusManager::refreshSignalOnly()')}
${extractFunction(source, 'void SystemStatusManager::refreshAllStatus()')}
${extractFunction(source, 'void SystemStatusManager::queryAllStatus()')}
${extractFunction(source, 'void SystemStatusManager::querySignalStrength()')}
${extractFunction(source, 'void SystemStatusManager::querySIMStatus()')}
${extractFunction(source, 'void SystemStatusManager::queryNetworkStatus()')}
${extractFunction(source, 'void SystemStatusManager::queryDataStatus()')}
${extractFunction(source, 'void SystemStatusManager::queryOperatorInfo()')}
int main() {
  SystemStatusManager::initStatus();
  responses = {
    {"AT+CSQ", "\\r\\n+CSQ: 20,99\\r\\nOK\\r\\n"},
    {"AT+CIMI", "\\r\\n234101234567890\\r\\nOK\\r\\n"},
    {"AT+CEREG?", "\\r\\n+CEREG: 2,5\\r\\nOK\\r\\n"},
    {"AT+CREG?", "\\r\\n+CREG: 2,0\\r\\nOK\\r\\n"},
    {"AT+CGATT?", "\\r\\n+CGATT: 1\\r\\nOK\\r\\n"},
    {"AT+COPS?", "\\r\\n+COPS: 0,2,\\"23410\\",7\\r\\nOK\\r\\n"},
  };
  simState = SIM_STATE_READY;
  SystemStatusManager::refreshAllStatus();
  auto status = SystemStatusManager::getStatus();
  assert(commands.size() == 6 && status.simReady);
  assert(status.signalStrength == -73 && status.networkConnected && status.epsRegistered && !status.csRegistered);
  assert(status.isRoaming && status.dataAttached && status.homeOperatorCode == "23410");
  assert(status.operatorCode == "23410" && status.networkType == "4G");
  responses["AT+CSQ"] = "+CSQ: 0,0\\r\\nERROR\\r\\n";
  responses["AT+CEREG?"] = "+CEREG: 2,0\\r\\nERROR\\r\\n";
  responses["AT+CREG?"] = "ERROR\\r\\n";
  responses["AT+CGATT?"] = "+CGATT: invalid\\r\\nOK\\r\\n";
  responses["AT+COPS?"] = "+COPS: 0,2,\\"46000\\",0\\r\\nERROR\\r\\n";
  SystemStatusManager::refreshAllStatus();
  status = SystemStatusManager::getStatus();
  assert(status.signalStrength == -73 && status.epsRegistered && status.isRoaming && status.dataAttached);
  assert(status.operatorCode == "23410" && status.networkType == "4G");
  const auto count = commands.size();
  busy = true;
  SystemStatusManager::refreshAllStatus();
  SystemStatusManager::refreshSignalOnly();
  assert(commands.size() == count);
  busy = false;
  int registration = -1;
  assert(parseRegStatFromResponse("+CEREG: 2,1\\r\\nOK\\r\\n", "+CEREG:", registration) && registration == 1);
  assert(!parseRegStatFromResponse("+CEREG: 2,1x\\r\\nOK\\r\\n", "+CEREG:", registration));
  simState = SIM_STATE_IDLE;
  SystemStatusManager::refreshAllStatus();
  status = SystemStatusManager::getStatus();
  assert(!status.networkConnected && !status.simReady && status.signalStrength == -999);
  std::cout << "Modem status shared transaction and complete-response tests passed.\\n";
}
`);