const fs = require('node:fs');
const path = require('node:path');
const { runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'bootstrap_credentials.cpp'), 'utf8');
runCpp(`
#include "${path.join(root, 'bootstrap_credentials.h')}"
#include "${path.join(root, 'config_manager.h')}"
#include <cassert>
#include <iostream>
#include <map>
Config config{};
bool saveOK = true;
bool saveConfig() { return saveOK; }
struct {
  std::string input;
  std::string output;
  int available() { return input.size(); }
  int read() { char value = input[0]; input.erase(0, 1); return value; }
  void print(const String& text) { output += text; }
  void println(const String& text) { output += text; output += '\\n'; }
} Serial;
std::map<std::string, String> nvs;
bool failNvs = false;
struct Preferences {
  bool begin(const char*, bool readOnly) { assert(readOnly); return !failNvs; }
  String getString(const char* key, const char* fallback) { return nvs.count(key) ? nvs[key] : String(fallback); }
  void end() {}
};
${source.slice(source.indexOf('static const String bootstrapWebPassword'))}
int main() {
  assert(initBootstrapCredentials());
  assert(getBootstrapWebPassword() == "admin1234" && getSetupAPPassword() == "12345678");
  assert(nvs.empty());
  bootstrapReady = false;
  assert(initBootstrapCredentials());
  assert(getBootstrapWebPassword() == "admin1234" && getSetupAPPassword() == "12345678");
  assert(!needsBootstrapWebAuth("admin1234"));
  assert(needsBootstrapWebAuth(""));
  assert(!needsBootstrapWebAuth("configured-custom-password"));
  const String previousRandomPassword = "00112233445566778899aabbccddeeff";
  nvs["web"] = previousRandomPassword;
  nvs["ap"] = "ffeeddccbbaa99887766554433221100";
  bootstrapReady = false;
  assert(initBootstrapCredentials());
  assert(getBootstrapWebPassword() == "admin1234" && getSetupAPPassword() == "12345678");
  assert(needsBootstrapWebAuth(previousRandomPassword));
  assert(!needsBootstrapWebAuth("00112233445566778899aabbccddeefa"));
  assert(nvs["web"] == previousRandomPassword);
  config.webAuth.password = "configured-custom-password";
  config.webAuth.username = "custom";
  config.webAuth.enabled = false;
  printBootstrapAccess();
  assert(Serial.output.empty());
  Serial.input = "RESET WEB AUTH\\n";
  saveOK = false;
  pollBootstrapRecovery();
  assert(config.webAuth.password == "configured-custom-password" && config.webAuth.username == "custom" && !config.webAuth.enabled);
  Serial.input = "RESET WEB AUTH\\n";
  saveOK = true;
  pollBootstrapRecovery();
  assert(config.webAuth.enabled && config.webAuth.username == "admin" && config.webAuth.password == "admin1234");
  assert(Serial.output.find("[SETUP] Web password: admin1234") != std::string::npos);
  bootstrapReady = false;
  nvs.clear();
  failNvs = true;
  assert(initBootstrapCredentials() && bootstrapReady);
  assert(getBootstrapWebPassword() == "admin1234" && getSetupAPPassword() == "12345678");
  assert(!needsBootstrapWebAuth(previousRandomPassword));
  std::cout << "Default Web/AP credentials and physical recovery tests passed.\\n";
}
`);