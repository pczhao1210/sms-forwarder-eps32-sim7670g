#include "config_manager.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>

#include "sms_filter.h"

Config config;
static bool spiffsInitialized = false;
static const char* CONFIG_PATH = "/config.json";

static bool configSectionExists(JsonVariantConst section) {
  return !section.isNull();
}

template <typename T>
static void assignIfPresent(JsonVariantConst section, const char* key, T& target) {
  if (!configSectionExists(section)) return;
  JsonVariantConst value = section[key];
  if (!value.isNull()) {
    target = value.as<T>();
  }
}

template <typename TDoc>
static void populateConfigDocument(TDoc& doc, bool includeSecrets, bool includeWebAuthPassword) {
  doc["lang"] = config.lang;

  JsonObject wifi = doc["wifi"].template to<JsonObject>();
  wifi["ssid"] = config.wifi.ssid;
  wifi["password"] = includeSecrets ? config.wifi.password : "";
  wifi["useCustomDns"] = config.wifi.useCustomDns;
  wifi["forceStaticDns"] = config.wifi.forceStaticDns;
  wifi["staticIp"] = config.wifi.staticIp;
  wifi["staticGateway"] = config.wifi.staticGateway;
  wifi["staticSubnet"] = config.wifi.staticSubnet;
  wifi["dns1"] = config.wifi.dns1;
  wifi["dns2"] = config.wifi.dns2;

  JsonObject bark = doc["bark"].template to<JsonObject>();
  bark["enabled"] = config.bark.enabled;
  bark["key"] = includeSecrets ? config.bark.key : "";
  bark["url"] = config.bark.url;

  JsonObject serverChan = doc["serverChan"].template to<JsonObject>();
  serverChan["enabled"] = config.serverChan.enabled;
  serverChan["key"] = includeSecrets ? config.serverChan.key : "";
  serverChan["url"] = config.serverChan.url;

  JsonObject telegram = doc["telegram"].template to<JsonObject>();
  telegram["enabled"] = config.telegram.enabled;
  telegram["token"] = includeSecrets ? config.telegram.token : "";
  telegram["chatId"] = includeSecrets ? config.telegram.chatId : "";
  telegram["url"] = config.telegram.url;

  JsonObject dingtalk = doc["dingtalk"].template to<JsonObject>();
  dingtalk["enabled"] = config.dingtalk.enabled;
  dingtalk["webhook"] = includeSecrets ? config.dingtalk.webhook : "";

  JsonObject feishu = doc["feishu"].template to<JsonObject>();
  feishu["enabled"] = config.feishu.enabled;
  feishu["webhook"] = includeSecrets ? config.feishu.webhook : "";

  JsonObject custom = doc["custom"].template to<JsonObject>();
  custom["enabled"] = config.custom.enabled;
  custom["url"] = includeSecrets ? config.custom.url : "";
  custom["key"] = includeSecrets ? config.custom.key : "";

  JsonObject battery = doc["battery"].template to<JsonObject>();
  battery["lowThreshold"] = config.battery.lowThreshold;
  battery["criticalThreshold"] = config.battery.criticalThreshold;
  battery["alertEnabled"] = config.battery.alertEnabled;
  battery["chargingAlertEnabled"] = config.battery.chargingAlertEnabled;
  battery["lowBatteryAlertEnabled"] = config.battery.lowBatteryAlertEnabled;
  battery["fullChargeAlertEnabled"] = config.battery.fullChargeAlertEnabled;

  JsonObject sleep = doc["sleep"].template to<JsonObject>();
  sleep["enabled"] = config.sleep.enabled;
  sleep["timeout"] = config.sleep.timeout;
  sleep["mode"] = config.sleep.mode;

  JsonObject network = doc["network"].template to<JsonObject>();
  network["roamingAlertEnabled"] = config.network.roamingAlertEnabled;
  network["autoDisableDataRoaming"] = config.network.autoDisableDataRoaming;
  network["allowSmsDataRoaming"] = config.network.allowSmsDataRoaming;
  network["signalCheckInterval"] = config.network.signalCheckInterval;
  network["operatorMode"] = config.network.operatorMode;
  network["radioMode"] = config.network.radioMode;
  network["apn"] = config.network.apn;
  network["apnUser"] = includeSecrets ? config.network.apnUser : "";
  network["apnPass"] = includeSecrets ? config.network.apnPass : "";
  network["dataPolicy"] = config.network.dataPolicy;

  JsonObject smsFilter = doc["smsFilter"].template to<JsonObject>();
  smsFilter["whitelistEnabled"] = config.smsFilter.whitelistEnabled;
  smsFilter["keywordFilterEnabled"] = config.smsFilter.keywordFilterEnabled;
  smsFilter["whitelist"] = config.smsFilter.whitelist;
  smsFilter["blockedKeywords"] = config.smsFilter.blockedKeywords;

  JsonObject reporting = doc["reporting"].template to<JsonObject>();
  reporting["dailyReportEnabled"] = config.reporting.dailyReportEnabled;
  reporting["weeklyReportEnabled"] = config.reporting.weeklyReportEnabled;
  reporting["reportHour"] = config.reporting.reportHour;

  JsonObject debug = doc["debug"].template to<JsonObject>();
  debug["atCommandEcho"] = config.debug.atCommandEcho;

  JsonObject watchdog = doc["watchdog"].template to<JsonObject>();
  watchdog["timeout"] = config.watchdog.timeout;

  JsonObject ota = doc["ota"].template to<JsonObject>();
  ota["enabled"] = config.ota.enabled;
  ota["hostname"] = config.ota.hostname;
  ota["password"] = includeSecrets ? config.ota.password : "";

  JsonObject webAuth = doc["webAuth"].template to<JsonObject>();
  webAuth["enabled"] = config.webAuth.enabled;
  webAuth["username"] = config.webAuth.username;
  webAuth["hasPassword"] = !config.webAuth.password.isEmpty();
  if (includeWebAuthPassword) {
    webAuth["password"] = config.webAuth.password;
  } else {
    webAuth["password"] = "";
  }
}

void initConfig() {
  Serial.println("初始化SPIFFS...");
  if (!spiffsInitialized) {
    if (!SPIFFS.begin(false)) {
      Serial.println("SPIFFS首次初始化失败，尝试格式化...");
      if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS格式化失败，无法使用SPIFFS");
        return;
      }
      Serial.println("SPIFFS格式化成功");
    } else {
      Serial.println("SPIFFS初始化成功");
    }
    spiffsInitialized = true;
  }

  loadConfig();
}

void loadConfig() {
  setDefaultConfig();

  if (!spiffsInitialized) {
    Serial.println("SPIFFS未初始化，无法加载配置");
    smsFilter.loadFromConfigStrings(config.smsFilter.whitelist, config.smsFilter.blockedKeywords);
    return;
  }

  File file = SPIFFS.open(CONFIG_PATH, "r");
  if (!file) {
    Serial.println("配置文件不存在，使用默认配置");
    saveConfig();
    smsFilter.loadFromConfigStrings(config.smsFilter.whitelist, config.smsFilter.blockedKeywords);
    return;
  }

  DynamicJsonDocument doc(16384);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("配置文件损坏，恢复默认配置");
    saveConfig();
    smsFilter.loadFromConfigStrings(config.smsFilter.whitelist, config.smsFilter.blockedKeywords);
    return;
  }

  assignIfPresent(doc.as<JsonVariantConst>(), "lang", config.lang);

  JsonVariantConst wifi = doc["wifi"];
  assignIfPresent(wifi, "ssid", config.wifi.ssid);
  assignIfPresent(wifi, "password", config.wifi.password);
  assignIfPresent(wifi, "useCustomDns", config.wifi.useCustomDns);
  assignIfPresent(wifi, "forceStaticDns", config.wifi.forceStaticDns);
  assignIfPresent(wifi, "staticIp", config.wifi.staticIp);
  assignIfPresent(wifi, "staticGateway", config.wifi.staticGateway);
  assignIfPresent(wifi, "staticSubnet", config.wifi.staticSubnet);
  assignIfPresent(wifi, "dns1", config.wifi.dns1);
  assignIfPresent(wifi, "dns2", config.wifi.dns2);

  JsonVariantConst bark = doc["bark"];
  assignIfPresent(bark, "enabled", config.bark.enabled);
  assignIfPresent(bark, "key", config.bark.key);
  assignIfPresent(bark, "url", config.bark.url);

  JsonVariantConst serverChan = doc["serverChan"];
  assignIfPresent(serverChan, "enabled", config.serverChan.enabled);
  assignIfPresent(serverChan, "key", config.serverChan.key);
  assignIfPresent(serverChan, "url", config.serverChan.url);

  JsonVariantConst telegram = doc["telegram"];
  assignIfPresent(telegram, "enabled", config.telegram.enabled);
  assignIfPresent(telegram, "token", config.telegram.token);
  assignIfPresent(telegram, "chatId", config.telegram.chatId);
  assignIfPresent(telegram, "url", config.telegram.url);

  JsonVariantConst dingtalk = doc["dingtalk"];
  assignIfPresent(dingtalk, "enabled", config.dingtalk.enabled);
  assignIfPresent(dingtalk, "webhook", config.dingtalk.webhook);

  JsonVariantConst feishu = doc["feishu"];
  assignIfPresent(feishu, "enabled", config.feishu.enabled);
  assignIfPresent(feishu, "webhook", config.feishu.webhook);

  JsonVariantConst custom = doc["custom"];
  assignIfPresent(custom, "enabled", config.custom.enabled);
  assignIfPresent(custom, "url", config.custom.url);
  assignIfPresent(custom, "key", config.custom.key);

  JsonVariantConst smsFilterSection = doc["smsFilter"];
  assignIfPresent(smsFilterSection, "whitelistEnabled", config.smsFilter.whitelistEnabled);
  assignIfPresent(smsFilterSection, "keywordFilterEnabled", config.smsFilter.keywordFilterEnabled);
  assignIfPresent(smsFilterSection, "whitelist", config.smsFilter.whitelist);
  assignIfPresent(smsFilterSection, "blockedKeywords", config.smsFilter.blockedKeywords);

  JsonVariantConst reporting = doc["reporting"];
  assignIfPresent(reporting, "dailyReportEnabled", config.reporting.dailyReportEnabled);
  assignIfPresent(reporting, "weeklyReportEnabled", config.reporting.weeklyReportEnabled);
  assignIfPresent(reporting, "reportHour", config.reporting.reportHour);

  JsonVariantConst battery = doc["battery"];
  assignIfPresent(battery, "lowThreshold", config.battery.lowThreshold);
  assignIfPresent(battery, "criticalThreshold", config.battery.criticalThreshold);
  assignIfPresent(battery, "alertEnabled", config.battery.alertEnabled);
  assignIfPresent(battery, "lowBatteryAlertEnabled", config.battery.lowBatteryAlertEnabled);
  assignIfPresent(battery, "chargingAlertEnabled", config.battery.chargingAlertEnabled);
  assignIfPresent(battery, "fullChargeAlertEnabled", config.battery.fullChargeAlertEnabled);

  JsonVariantConst sleep = doc["sleep"];
  assignIfPresent(sleep, "enabled", config.sleep.enabled);
  assignIfPresent(sleep, "timeout", config.sleep.timeout);
  assignIfPresent(sleep, "mode", config.sleep.mode);

  JsonVariantConst ota = doc["ota"];
  assignIfPresent(ota, "enabled", config.ota.enabled);
  assignIfPresent(ota, "hostname", config.ota.hostname);
  assignIfPresent(ota, "password", config.ota.password);

  JsonVariantConst network = doc["network"];
  assignIfPresent(network, "roamingAlertEnabled", config.network.roamingAlertEnabled);
  assignIfPresent(network, "autoDisableDataRoaming", config.network.autoDisableDataRoaming);
  assignIfPresent(network, "allowSmsDataRoaming", config.network.allowSmsDataRoaming);
  assignIfPresent(network, "signalCheckInterval", config.network.signalCheckInterval);
  assignIfPresent(network, "operatorMode", config.network.operatorMode);
  assignIfPresent(network, "radioMode", config.network.radioMode);
  assignIfPresent(network, "apn", config.network.apn);
  assignIfPresent(network, "apnUser", config.network.apnUser);
  assignIfPresent(network, "apnPass", config.network.apnPass);
  assignIfPresent(network, "dataPolicy", config.network.dataPolicy);

  JsonVariantConst debug = doc["debug"];
  assignIfPresent(debug, "atCommandEcho", config.debug.atCommandEcho);

  JsonVariantConst watchdog = doc["watchdog"];
  assignIfPresent(watchdog, "timeout", config.watchdog.timeout);

  JsonVariantConst webAuth = doc["webAuth"];
  assignIfPresent(webAuth, "enabled", config.webAuth.enabled);
  assignIfPresent(webAuth, "username", config.webAuth.username);
  assignIfPresent(webAuth, "password", config.webAuth.password);

  Serial.println("配置加载完成");
  smsFilter.loadFromConfigStrings(config.smsFilter.whitelist, config.smsFilter.blockedKeywords);
}

void saveConfig() {
  if (!spiffsInitialized) {
    Serial.println("SPIFFS未初始化，无法保存配置");
    return;
  }

  File file = SPIFFS.open(CONFIG_PATH, "w");
  if (!file) {
    Serial.println("无法创建配置文件");
    return;
  }

  DynamicJsonDocument doc(16384);
  populateConfigDocument(doc, true, true);
  serializeJson(doc, file);
  file.close();
  Serial.println("配置已保存");
}

String exportConfigAsJson(bool includeSecrets, bool includeWebAuthPassword) {
  DynamicJsonDocument doc(16384);
  populateConfigDocument(doc, includeSecrets, includeWebAuthPassword);
  String json;
  serializeJson(doc, json);
  return json;
}

void setDefaultConfig() {
  config.lang = "auto";

  config.wifi.ssid = "SMS-Forwarder";
  config.wifi.password = "12345678";
  config.wifi.useCustomDns = false;
  config.wifi.forceStaticDns = false;
  config.wifi.staticIp = "";
  config.wifi.staticGateway = "";
  config.wifi.staticSubnet = "";
  config.wifi.dns1 = "";
  config.wifi.dns2 = "";

  config.bark.enabled = false;
  config.bark.key = "";
  config.bark.url = "https://api.day.app";

  config.serverChan.enabled = false;
  config.serverChan.key = "";
  config.serverChan.url = "https://sctapi.ftqq.com";

  config.telegram.enabled = false;
  config.telegram.token = "";
  config.telegram.chatId = "";
  config.telegram.url = "https://api.telegram.org";

  config.dingtalk.enabled = false;
  config.dingtalk.webhook = "";

  config.feishu.enabled = false;
  config.feishu.webhook = "";

  config.custom.enabled = false;
  config.custom.url = "";
  config.custom.key = "";

  config.battery.lowThreshold = 15;
  config.battery.criticalThreshold = 5;
  config.battery.alertEnabled = true;
  config.battery.chargingAlertEnabled = false;
  config.battery.lowBatteryAlertEnabled = true;
  config.battery.fullChargeAlertEnabled = false;

  config.sleep.enabled = false;
  config.sleep.timeout = 1800;
  config.sleep.mode = 1;

  config.network.roamingAlertEnabled = true;
  config.network.autoDisableDataRoaming = true;
  config.network.allowSmsDataRoaming = false;
  config.network.signalCheckInterval = 30;
  config.network.operatorMode = 0;
  config.network.radioMode = 38;
  config.network.apn = "";
  config.network.apnUser = "";
  config.network.apnPass = "";
  config.network.dataPolicy = DATA_POLICY_ALWAYS_OFF;

  config.smsFilter.whitelistEnabled = false;
  config.smsFilter.keywordFilterEnabled = false;
  config.smsFilter.whitelist = "";
  config.smsFilter.blockedKeywords = "";

  config.reporting.dailyReportEnabled = false;
  config.reporting.weeklyReportEnabled = false;
  config.reporting.reportHour = 9;

  config.debug.atCommandEcho = false;
  config.watchdog.timeout = 30;

  config.ota.enabled = true;
  config.ota.hostname = "sms-forwarder";
  config.ota.password = "admin";

  config.webAuth.enabled = true;
  config.webAuth.username = "admin";
  config.webAuth.password = "admin1234";
}
