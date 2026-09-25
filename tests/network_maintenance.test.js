const fs = require('node:fs');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'network_manager.cpp'), 'utf8');
const header = fs.readFileSync(path.join(root, 'network_manager.h'), 'utf8');
const types = header.slice(header.indexOf('struct NetworkInfo'), header.indexOf('extern SMSNetworkManager'))
  .replace('private:', 'public:');
const globals = source.slice(source.indexOf('bool SMSNetworkManager::last_roaming_status'),
  source.indexOf('static void deferNetworkMaintenance'));
const signatures = [
  'static void deferNetworkMaintenance()',
  'static bool deferIfModemBusy(',
  'static bool responseHasOk(',
  'static bool responseHasCgattState(',
  'static bool responseHasCgactState(',
  'static bool isCidActiveCounterError(',
  'static bool queryDataStateSnapshot(',
  'static bool hasOperatorInfo(',
  'static int normalizeDataPolicy(',
  'static bool shouldEnableDataForPolicy(',
  'void SMSNetworkManager::initNetwork()',
  'NetworkInfo SMSNetworkManager::getNetworkInfo()',
  'bool SMSNetworkManager::detectRoaming()',
  'bool SMSNetworkManager::setDataConnection(',
  'void SMSNetworkManager::checkNetworkStatus()',
];
runCpp(`
#include <Arduino.h>
#include <cassert>
#include <deque>
#include <iostream>
#include "${path.join(root, 'millis_utils.h')}"
#include "${path.join(root, 'network_policy.h')}"
#include "${path.join(root, 'at_response.h')}"
std::vector<String> errors, info, debug;
#define LOGE(tag, key, ...) errors.push_back(key)
#define LOGW(tag, key, ...) ((void)0)
#define LOGI(tag, key, ...) info.push_back(key)
#define LOGD(tag, key, ...) debug.push_back(key)
const int LOG_WARN = 1;
struct { void addLog(int, const char*, const char*) {} } logManager;
uint32_t tick = 60000;
uint32_t millis() { return tick; }
enum { SIM_STATE_READY, SIM_STATE_IDLE };
int simState = SIM_STATE_READY;
bool busy = false, pdpReady = true, busyOnStatus = false;
bool isModemAvailableForMaintenance() { return simState == SIM_STATE_READY && !busy; }
bool isPdpConfigurationReady() { return pdpReady; }
struct {
  struct {
    int signalCheckInterval = 30, dataPolicy = DATA_POLICY_ALWAYS_OFF;
    bool autoDisableDataRoaming = true, allowSmsDataRoaming = false, roamingAlertEnabled = false;
  } network;
} config;
struct SystemStatus {
  String operatorCode = "23410", homeOperatorCode = "23410";
  String operatorName = "carrier", homeOperatorName = "carrier", networkType = "4G";
  int signalStrength = -70;
  bool csRegistered = true, epsRegistered = true, isRoaming = false, networkConnected = true;
};
struct {
  SystemStatus status;
  int updates = 0;
  void updateStatus() { updates++; if (busyOnStatus) busy = true; }
  SystemStatus getStatus() { return status; }
} systemStatus;
std::deque<String> responses;
std::vector<String> commands;
String sendATCommand(const String& command) {
  assert(!busy);
  assert(!responses.empty());
  commands.push_back(command);
  String response = responses.front();
  responses.pop_front();
  return response;
}
${types}
${globals}
bool SMSNetworkManager::sendRoamingAlert(const NetworkInfo&) { return true; }
${signatures.map(signature => extractFunction(source, signature)).join('\n')}
void reset() {
  SMSNetworkManager::initNetwork();
  busy = false;
  busyOnStatus = false;
  pdpReady = true;
  simState = SIM_STATE_READY;
  tick = 60000;
  config.network.dataPolicy = DATA_POLICY_ALWAYS_OFF;
  systemStatus.status = {};
  systemStatus.updates = 0;
  commands.clear();
  responses.clear();
  errors.clear();
  info.clear();
  debug.clear();
}
int main() {
  reset();
  busy = true;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.empty() && errors.empty() && networkMaintenanceDeferred);
  assert(systemStatus.updates == 0 && lastNetworkCheckMs == 0);
  auto logs = debug.size();
  SMSNetworkManager::checkNetworkStatus();
  assert(debug.size() == logs);
  busy = false;
  responses = {"OK"};
  tick += 999;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.empty());
  tick++;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands == std::vector<String>{"AT+CGACT=0,1"});
  assert(SMSNetworkManager::data_state_known && !SMSNetworkManager::data_connection_enabled);
  assert(!networkMaintenanceDeferred && errors.empty());
  tick += 29999;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.size() == 1 && systemStatus.updates == 1);

  reset();
  busy = true;
  SMSNetworkManager::checkNetworkStatus();
  logs = debug.size();
  for (int attempt = 0; attempt < 3; attempt++) {
    tick += 1000;
    SMSNetworkManager::checkNetworkStatus();
    assert(commands.empty() && errors.empty() && debug.size() == logs);
  }
  simState = SIM_STATE_IDLE;
  tick += 1000;
  SMSNetworkManager::checkNetworkStatus();
  logs = debug.size();
  SMSNetworkManager::checkNetworkStatus();
  assert(debug.size() == logs);
  busy = false;
  simState = SIM_STATE_READY;
  responses = {"OK"};
  tick += 1000;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.size() == 1 && !networkMaintenanceDeferred);

  reset();
  SMSNetworkManager::data_state_known = true;
  SMSNetworkManager::data_connection_enabled = true;
  busy = true;
  assert(!SMSNetworkManager::setDataConnection(false));
  assert(commands.empty() && SMSNetworkManager::data_state_known && SMSNetworkManager::data_connection_enabled);
  busy = false;
  responses = {"BUSY: modem busy"};
  assert(!SMSNetworkManager::setDataConnection(false));
  assert(commands.size() == 1 && errors.empty() && SMSNetworkManager::data_state_known);
  tick += 1000;
  responses = {"OK"};
  SMSNetworkManager::checkNetworkStatus();
  assert(!SMSNetworkManager::data_connection_enabled && !networkMaintenanceDeferred);

  reset();
  config.network.dataPolicy = DATA_POLICY_ALWAYS_ON;
  responses = {"OK", "BUSY: AT in progress"};
  assert(!SMSNetworkManager::setDataConnection(true));
  assert((commands == std::vector<String>{"AT+CGATT=1", "AT+CGACT=1,1"}));
  assert(errors.empty() && !SMSNetworkManager::data_state_known && networkMaintenanceDeferred);
  config.network.dataPolicy = DATA_POLICY_ALWAYS_OFF;
  responses = {"OK"};
  tick += 1000;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.back() == "AT+CGACT=0,1" && commands.size() == 3);

  reset();
  responses = {"BUSY: modem busy"};
  assert(!SMSNetworkManager::setDataConnection(true));
  assert(commands == std::vector<String>{"AT+CGATT=1"});
  assert(errors.empty() && networkMaintenanceDeferred);
  reset();
  responses = {"ERROR", "BUSY: modem busy"};
  assert(!SMSNetworkManager::setDataConnection(false));
  assert((commands == std::vector<String>{"AT+CGACT=0,1", "AT+CGATT?"}));
  assert(errors.empty() && networkMaintenanceDeferred);
  reset();
  responses = {"ERROR", "+CGATT: 1\\r\\nOK", "BUSY: modem busy"};
  assert(!SMSNetworkManager::setDataConnection(false));
  assert(commands.size() == 3 && errors.empty() && networkMaintenanceDeferred);

  reset();
  config.network.dataPolicy = DATA_POLICY_ALWAYS_ON;
  SMSNetworkManager::data_state_known = true;
  SMSNetworkManager::data_connection_enabled = true;
  responses = {"BUSY: modem busy"};
  SMSNetworkManager::checkNetworkStatus();
  assert(commands == std::vector<String>{"AT+CGATT?"});
  assert(errors.empty() && SMSNetworkManager::data_state_known && lastDataCheckMs == 0);
  responses = {"+CGATT: 1\\r\\nOK", "BUSY: modem busy"};
  tick += 1000;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.size() == 3 && networkMaintenanceDeferred && SMSNetworkManager::data_state_known);
  responses = {"+CGATT: 1\\r\\nOK", "+CGACT: 1,1\\r\\nOK"};
  tick += 1000;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.size() == 5 && !networkMaintenanceDeferred && lastDataCheckMs == tick);
  assert(errors.empty());

  reset();
  busyOnStatus = true;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.empty() && networkMaintenanceDeferred && errors.empty());
  busyOnStatus = false;
  busy = false;
  responses = {"OK"};
  tick += 1000;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.size() == 1 && !networkMaintenanceDeferred);

  reset();
  systemStatus.status.networkConnected = false;
  responses = {"BUSY: modem busy"};
  SMSNetworkManager::checkNetworkStatus();
  assert(commands == std::vector<String>{"AT+COPS=0"});
  assert(SMSNetworkManager::last_check_time == 0 && networkMaintenanceDeferred && errors.empty());
  responses = {"OK", "OK"};
  tick += 1000;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.size() == 3 && SMSNetworkManager::last_check_time == tick);

  reset();
  systemStatus.status.isRoaming = true;
  SMSNetworkManager::data_state_known = true;
  SMSNetworkManager::data_connection_enabled = true;
  busy = true;
  SMSNetworkManager::detectRoaming();
  assert(commands.empty() && networkMaintenanceDeferred && errors.empty());
  busy = false;
  responses = {"OK"};
  tick += 1000;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands == std::vector<String>{"AT+CGACT=0,1"});

  reset();
  responses = {"+CME ERROR: Last PDN disconnection not allowed", "+CGATT: 1\\r\\nOK", "+CGACT: 1,0\\r\\nOK"};
  assert(SMSNetworkManager::setDataConnection(false));
  assert(SMSNetworkManager::data_state_known && errors.empty());
  reset();
  responses = {"OK", "ERROR", "+CGATT: 1\\r\\nOK", "+CGACT: 1,1\\r\\nERROR"};
  assert(!SMSNetworkManager::setDataConnection(true));
  assert(errors == std::vector<String>{"data_switch_fail"});
  assert(!SMSNetworkManager::data_state_known && !networkMaintenanceDeferred);

  reset();
  tick = UINT32_MAX - 500;
  busy = true;
  SMSNetworkManager::checkNetworkStatus();
  busy = false;
  responses = {"OK"};
  tick += 999;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.empty());
  tick++;
  SMSNetworkManager::checkNetworkStatus();
  assert(commands.size() == 1 && !networkMaintenanceDeferred);
  std::cout << "Network maintenance BUSY deferral and state preservation tests passed.\\n";
}
`);
