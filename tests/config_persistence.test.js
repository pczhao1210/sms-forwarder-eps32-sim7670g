const fs = require('node:fs');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'config_manager.cpp'), 'utf8');
const credentials = fs.readFileSync(path.join(root, 'bootstrap_credentials.cpp'), 'utf8');
if (!process.env.ARDUINOJSON_HEADER) throw new Error('Set ARDUINOJSON_HEADER to ArduinoJson v6 single-header path');
runCpp(`
#include <cassert>
#include <iostream>
#include <ArduinoJson.h>
#include "${path.join(root, 'config_manager.h')}"
#include "${path.join(root, 'verified_file.h')}"
struct { void println(const char*) {} } Serial;
const String& getBootstrapWebPassword() { static String password = "admin1234"; return password; }
String legacyBootstrapWebPassword;
${extractFunction(credentials, 'bool needsBootstrapWebAuth(')}
struct { void loadFromConfigStrings(const String&, const String&) {} } smsFilter;
Config config;
bool spiffsInitialized = true;
const char* CONFIG_PATH = "/config.json";
const char* CONFIG_TMP_PATH = "/config.tmp";
const char* CONFIG_BACKUP_PATH = "/config.bak";
const char* CONFIG_CORRUPT_PATH = "/config.corrupt";
${extractFunction(source, 'static bool configSectionExists(')}
template <typename T>
${extractFunction(source, 'static void assignIfPresent(')}
template <typename T>
${extractFunction(source, 'static bool configFieldsHaveType(')}
${extractFunction(source, 'static bool readConfigDocument(')}
template <typename TDoc>
${extractFunction(source, 'static void populateConfigDocument(')}
${extractFunction(source, 'static int clampIntValue(')}
${extractFunction(source, 'static void normalizeConfigValues()')}
${extractFunction(source, 'void setDefaultConfig()')}
${extractFunction(source, 'bool saveConfig()')}
${extractFunction(source, 'void loadConfig()')}
${extractFunction(source, 'String exportConfigAsJson(')}
int main() {
  for (int failure = 0; failure < 4; failure++) {
    SPIFFS.files.clear();
    FakeFS::writeRemaining = std::numeric_limits<size_t>::max();
    FakeFS::corruptOnFlush = false;
    FakeFS::failRenameTo.clear();
    setDefaultConfig();
    assert(config.webAuth.enabled && config.webAuth.username == "admin" && config.webAuth.password == "admin1234");
    config.wifi.ssid = "old-config";
    assert(saveConfig());
    const auto original = *SPIFFS.files.at(CONFIG_PATH);
    config.wifi.ssid = "new-config";
    if (failure == 0) FakeFS::writeRemaining = 20;
    if (failure == 1) FakeFS::corruptOnFlush = true;
    if (failure == 2) FakeFS::failRenameTo = CONFIG_BACKUP_PATH;
    if (failure == 3) FakeFS::failRenameTo = CONFIG_PATH;
    assert(!saveConfig());
    assert(SPIFFS.exists(CONFIG_PATH) || SPIFFS.exists(CONFIG_BACKUP_PATH));
    DynamicJsonDocument doc(16384);
    const char* recovery = SPIFFS.exists(CONFIG_PATH) ? CONFIG_PATH : CONFIG_BACKUP_PATH;
    assert(*SPIFFS.files.at(recovery) == original);
    assert(readConfigDocument(recovery, doc));
    assert(doc["wifi"]["ssid"].as<std::string>() == "old-config");
  }
  FakeFS::failRenameTo.clear();
  FakeFS::writeRemaining = std::numeric_limits<size_t>::max();
  FakeFS::corruptOnFlush = false;
  DynamicJsonDocument doc(16384);
  for (const char* invalid : {"[]", "null", "42", "{\\"wifi\\":false}", "{\\"lang\\":7}",
      "{\\"webAuth\\":{\\"enabled\\":\\"false\\"}}", "{\\"webAuth\\":{\\"enabled\\":0}}",
      "{\\"wifi\\":{\\"password\\":null}}", "{\\"telegram\\":{\\"chatId\\":42}}",
      "{\\"battery\\":{\\"lowThreshold\\":20.5}}", "{\\"watchdog\\":{\\"timeout\\":2147483648}}",
      "{\\"reporting\\":{\\"reportHour\\":\\"0\\"}}", "{\\"network\\":{\\"dataPolicy\\":[]}}"}) {
    SPIFFS.files[CONFIG_PATH] = std::make_shared<std::string>(invalid);
    assert(!readConfigDocument(CONFIG_PATH, doc));
  }
  SPIFFS.files[CONFIG_PATH] = std::make_shared<std::string>("{\\"wifi\\":{\\"ssid\\":\\"partial\\"},\\"webAuth\\":{\\"enabled\\":false},\\"reporting\\":{\\"reportHour\\":0}}");
  assert(readConfigDocument(CONFIG_PATH, doc));
  config.wifi.password = "SECRET_WIFI";
  config.bark.key = "SECRET_BARK";
  config.bark.url = "https://host/path?SECRET_URL";
  config.custom.url = "https://host/SECRET_CUSTOM";
  config.network.apnPass = "SECRET_APN";
  config.webAuth.password = "SECRET_AUTH";
  String redacted = exportConfigAsJson(false, false);
  assert(redacted.find("SECRET_") == std::string::npos);
  assert(deserializeJson(doc, redacted) == DeserializationError::Ok);
  assert(doc["wifi"]["hasPassword"].as<bool>());
  assert(doc["bark"]["hasKey"].as<bool>() && doc["bark"]["hasUrl"].as<bool>());
  assert(doc["network"]["hasApnPass"].as<bool>());
  assert(exportConfigAsJson(true, true).find("SECRET_WIFI") != std::string::npos);
  legacyBootstrapWebPassword = "00112233445566778899aabbccddeeff";
  for (const char* savedPassword : {"admin1234", "custom-password", "00112233445566778899aabbccddeeff", "ffeeddccbbaa99887766554433221100", ""}) {
    SPIFFS.files.clear();
    SPIFFS.files["/sms.json"] = std::make_shared<std::string>("preserved-sms-history");
    setDefaultConfig();
    config.wifi.ssid = "preserved-wifi";
    config.wifi.password = "preserved-wifi-password";
    config.webAuth.username = "custom-admin";
    config.webAuth.password = savedPassword;
    config.webAuth.enabled = false;
    assert(saveConfig());
    const bool migrate = needsBootstrapWebAuth(savedPassword);
    loadConfig();
    assert(config.webAuth.username == "custom-admin");
    assert(config.webAuth.password == (migrate ? "admin1234" : savedPassword));
    assert(config.webAuth.enabled == migrate);
    assert(config.wifi.ssid == "preserved-wifi" && config.wifi.password == "preserved-wifi-password");
    assert(*SPIFFS.files.at("/sms.json") == "preserved-sms-history");
    assert(readConfigDocument(CONFIG_PATH, doc));
    assert(doc["webAuth"]["password"].as<String>() == config.webAuth.password);
  }
  std::cout << "Configuration write verification and recovery tests passed.\\n";
}
`, [
  '-DARDUINOJSON_HEADER="' + process.env.ARDUINOJSON_HEADER + '"',
]);