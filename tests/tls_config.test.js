const fs = require('node:fs');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'web_server.cpp'), 'utf8');
if (!process.env.ARDUINOJSON_HEADER) throw new Error('Set ARDUINOJSON_HEADER to ArduinoJson v6 single-header path');

runCpp(`
#include <cassert>
#include <cerrno>
#include <iostream>
#include <ArduinoJson.h>
#include "${path.join(root, 'config_manager.h')}"
#include "${path.join(root, 'input_validation.h')}"

struct {
  std::map<std::string, String> params;
  int status = 0;
  String body;
  bool hasArg(const String& key) const { return params.count(key); }
  String arg(const String& key) const {
    auto found = params.find(key);
    return found == params.end() ? String() : found->second;
  }
  String arg(int index) const { auto it = params.begin(); std::advance(it, index); return it->second; }
  String argName(int index) const { auto it = params.begin(); std::advance(it, index); return it->first; }
  int args() const { return static_cast<int>(params.size()); }
  void send(int code, const char*, const String& value) { status = code; body = value; }
} server;
template <typename TDoc>
void sendJsonDocument(int status, TDoc& doc) {
  String response;
  serializeJson(doc, response);
  assert(!doc.overflowed());
  server.send(status, "application/json", response);
}
const char* i18nGet(const char* key) { return key; }
String i18nFormat(const char* key, const char* value) { return String(key) + ": " + value; }
void touchActivity() {}
String redactWebParamForLog(const String&, const String&) { return "[redacted]"; }
#define LOGI(...) (void)0
#define LOGD(...) (void)0
Config config;
Config persisted;
int saves = 0, watchdogUpdates = 0, sleepUpdates = 0;
bool failSave = false, pendingWork = false;
bool saveConfig() {
  ++saves;
  if (failSave) return false;
  persisted = config;
  return true;
}
void loadConfig() { config = persisted; }
struct { bool hasPendingWork() { return pendingWork; } } notificationManager;
struct { void configure(bool, int, int) { ++sleepUpdates; } } sleepManager;
struct {
  void disableWatchdog() { ++watchdogUpdates; }
  void initWatchdog() { ++watchdogUpdates; }
} watchdogManager;

${extractFunction(source, 'static String jsonError(')}
${extractFunction(source, 'static bool saveConfigOrSendError()')}
${extractFunction(source, 'static void sendRangeError(')}
${extractFunction(source, 'static bool parseBoundedIntArg(')}
${extractFunction(source, 'static void sendAllowedValueError(const char* field) {')}
${extractFunction(source, 'static bool readSecretArg(')}
${extractFunction(source, 'static bool requireTlsWatchdogBudget(')}
${extractFunction(source, 'void handleSetNotificationConfig()')}
${extractFunction(source, 'void handleSetSystemConfig()')}

void reset() {
  config = {};
  config.watchdog.timeout = 30;
  config.sleep.timeout = 1800;
  config.sleep.mode = 1;
  config.webAuth.enabled = true;
  config.webAuth.username = "admin";
  config.webAuth.password = "saved-management-password";
  config.bark.enabled = true;
  config.bark.key = "saved-key";
  persisted = config;
  server.params.clear();
  server.params["bark-enabled"] = "true";
  server.params["web-auth-enabled"] = "true";
  server.params["web-auth-username"] = "admin";
  server.status = 0;
  server.body.clear();
  saves = watchdogUpdates = sleepUpdates = 0;
  failSave = pendingWork = false;
}
void notificationArgs(const char* seconds) {
  server.params["tlsHandshakeTimeoutSeconds"] = seconds;
  server.params["barkKeyAction"] = "replace";
  server.params["barkKey"] = "replacement-key";
}
void unchanged(int status) {
  assert(server.status == status && saves == 0);
  assert(config.tls.handshakeTimeoutSeconds == persisted.tls.handshakeTimeoutSeconds);
  assert(config.watchdog.timeout == persisted.watchdog.timeout);
  assert(config.bark.key == persisted.bark.key);
  assert(config.webAuth.enabled == persisted.webAuth.enabled);
  assert(watchdogUpdates == 0 && sleepUpdates == 0);
}
int main() {
  reset();
  handleSetNotificationConfig();
  assert(server.status == 200 && config.tls.handshakeTimeoutSeconds == 5);
  assert(config.bark.enabled && config.bark.key == "saved-key");
  reset();
  config.tls.handshakeTimeoutSeconds = 20;
  handleSetNotificationConfig();
  assert(server.status == 200 && persisted.tls.handshakeTimeoutSeconds == 20);

  for (const char* invalid : {"", "0", "-1", "61", "65537", "1.5", "abc", "999999999999999999999999"}) {
    reset();
    notificationArgs(invalid);
    handleSetNotificationConfig();
    unchanged(400);
    assert(server.body.find("invalid_range") != std::string::npos);
    assert(server.body.find("tlsHandshakeTimeoutSeconds") != std::string::npos);
  }
  reset();
  notificationArgs("26");
  handleSetNotificationConfig();
  unchanged(400);
  DynamicJsonDocument error(512);
  assert(!deserializeJson(error, server.body));
  assert(error["minimumWatchdogSeconds"].as<int>() == 31);
  assert(server.body.find("web_err_tls_watchdog") != std::string::npos);
  reset();
  notificationArgs("25");
  handleSetNotificationConfig();
  assert(server.status == 200 && config.tls.handshakeTimeoutSeconds == 25);
  assert(config.watchdog.timeout == 30 && watchdogUpdates == 0);

  for (int seconds : {1, 5, 20, 60}) {
    reset();
    config.watchdog.timeout = std::max(10, seconds + 5);
    server.params["tlsHandshakeTimeoutSeconds"] = String(seconds);
    server.params["privateCaHost"] = "EXAMPLE.INVALID";
    handleSetNotificationConfig();
    assert(server.status == 200 && saves == 1);
    assert(persisted.tls.handshakeTimeoutSeconds == seconds);
    assert(persisted.tls.privateCaHost == "EXAMPLE.INVALID");
    assert(config.bark.key == "saved-key");
  }
  reset();
  notificationArgs("60");
  config.watchdog.timeout = 64;
  persisted = config;
  handleSetNotificationConfig();
  unchanged(400);
  assert(!deserializeJson(error, server.body) && error["minimumWatchdogSeconds"].as<int>() == 65);

  reset();
  config.tls.handshakeTimeoutSeconds = 20;
  persisted = config;
  server.params["wdt-timeout"] = "24";
  handleSetSystemConfig();
  unchanged(400);
  assert(!deserializeJson(error, server.body) && error["minimumWatchdogSeconds"].as<int>() == 25);
  reset();
  config.watchdog.timeout = 60;
  persisted = config;
  pendingWork = true;
  server.params["wdt-timeout"] = "10";
  handleSetSystemConfig();
  unchanged(409);
  assert(server.body.find("web_err_watchdog_busy") != std::string::npos);
  server.params["wdt-timeout"] = "65";
  handleSetSystemConfig();
  assert(server.status == 200 && config.watchdog.timeout == 65 && watchdogUpdates == 2);
  reset();
  server.params["wdt-timeout"] = "10";
  handleSetSystemConfig();
  assert(server.status == 200 && config.watchdog.timeout == 10 && watchdogUpdates == 2);

  reset();
  failSave = true;
  notificationArgs("20");
  handleSetNotificationConfig();
  assert(server.status == 500 && config.tls.handshakeTimeoutSeconds == 5);
  assert(config.bark.key == "saved-key" && watchdogUpdates == 0);
  reset();
  failSave = true;
  server.params["wdt-timeout"] = "65";
  handleSetSystemConfig();
  assert(server.status == 500 && config.watchdog.timeout == 30 && watchdogUpdates == 0);
  std::cout << "TLS configuration API, bounds, watchdog conflict and rollback tests passed.\\n";
}
`, ['-DARDUINOJSON_HEADER="' + process.env.ARDUINOJSON_HEADER + '"']);
