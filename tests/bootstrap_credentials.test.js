const fs = require('node:fs');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'bootstrap_credentials.cpp'), 'utf8');
runCpp(`
#include "${path.join(root, 'bootstrap_credentials.h')}"
#include "${path.join(root, 'config_manager.h')}"
#include <cassert>
#include <iostream>
#include <map>
Config config{};
static String bootstrapWebPassword;
static String setupAPPassword;
static bool bootstrapReady = false;
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
  bool begin(const char*, bool) { return true; }
  String getString(const char* key, const char* fallback) { return nvs.count(key) ? nvs[key] : String(fallback); }
  size_t putString(const char* key, const String& value) { if (failNvs) return 0; nvs[key] = value; return value.length(); }
  void end() {}
};
int entropyCalls = 0;
void bootloader_random_enable() {}
void bootloader_random_disable() {}
void esp_fill_random(void* data, size_t size) { memset(data, ++entropyCalls, size); }
${extractFunction(source, 'static String generateBootstrapPassword()')}
${extractFunction(source, 'bool initBootstrapCredentials()')}
${extractFunction(source, 'void printBootstrapAccess()')}
${extractFunction(source, 'void pollBootstrapRecovery()')}
int main() {
  assert(initBootstrapCredentials());
  assert(bootstrapWebPassword.length() == 32 && setupAPPassword.length() == 32);
  assert(bootstrapWebPassword != setupAPPassword && entropyCalls == 2);
  const String saved = bootstrapWebPassword;
  bootstrapReady = false;
  assert(initBootstrapCredentials() && bootstrapWebPassword == saved && entropyCalls == 2);
  assert(needsBootstrapWebAuth("admin1234"));
  assert(needsBootstrapWebAuth(""));
  assert(!needsBootstrapWebAuth("configured-custom-password"));
  config.webAuth.password = "configured-custom-password";
  config.webAuth.username = "custom";
  Serial.input = "RESET WEB AUTH\\n";
  saveOK = false;
  pollBootstrapRecovery();
  assert(config.webAuth.password == "configured-custom-password");
  Serial.input = "RESET WEB AUTH\\n";
  saveOK = true;
  pollBootstrapRecovery();
  assert(config.webAuth.enabled && config.webAuth.username == "admin" && config.webAuth.password == saved);
  bootstrapReady = false;
  bootstrapWebPassword = "";
  setupAPPassword = "";
  nvs.clear();
  failNvs = true;
  assert(!initBootstrapCredentials() && !bootstrapReady);
  assert(bootstrapWebPassword.isEmpty() && setupAPPassword.isEmpty());
  std::cout << "Per-device credentials and physical recovery tests passed.\\n";
}
`);