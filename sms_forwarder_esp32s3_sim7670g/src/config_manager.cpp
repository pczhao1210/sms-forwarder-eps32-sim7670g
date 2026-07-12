#include "config_manager.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>

#include "sms_filter.h"

Config config;
static bool spiffsInitialized = false;
static const char* CONFIG_PATH = "/config.json";
static const char* CONFIG_TMP_PATH = "/config.tmp";
static const char* CONFIG_BACKUP_PATH = "/config.bak";
static const char* CONFIG_CORRUPT_PATH = "/config.corrupt";

static bool configSectionExists(JsonVariantConst section) {
  return !section.isNull();
}

static bool readConfigDocument(const char* path, DynamicJsonDocument& doc) {
  File file = SPIFFS.open(path, "r");
  if (!file) return false;

  doc.clear();
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  return !error && !doc.overflowed();
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

  JsonObject timeConfig = doc["time"].template to<JsonObject>();
  timeConfig["timezoneOffsetMinutes"] = config.time.timezoneOffsetMinutes;

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

  JsonObject led = doc["led"].template to<JsonObject>();
  led["enabled"] = config.led.enabled;
  led["brightness"] = config.led.brightness;
  led["quietHoursEnabled"] = config.led.quietHoursEnabled;
  led["quietStartMinutes"] = config.led.quietStartMinutes;
  led["quietEndMinutes"] = config.led.quietEndMinutes;

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

static int clampIntValue(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

static void normalizeConfigValues() {
  config.time.timezoneOffsetMinutes = clampIntValue(config.time.timezoneOffsetMinutes, -720, 840);

  config.battery.lowThreshold = clampIntValue(config.battery.lowThreshold, 5, 50);
  config.battery.criticalThreshold = clampIntValue(config.battery.criticalThreshold, 1, 20);
  if (config.battery.criticalThreshold >= config.battery.lowThreshold) {
    config.battery.criticalThreshold = config.battery.lowThreshold - 1;
    if (config.battery.criticalThreshold < 1) {
      config.battery.criticalThreshold = 1;
    }
  }

  config.network.signalCheckInterval = clampIntValue(config.network.signalCheckInterval, 10, 300);
  config.network.operatorMode = clampIntValue(config.network.operatorMode, 0, 4);
  if (config.network.radioMode != 2 && config.network.radioMode != 38) {
    config.network.radioMode = 38;
  }
  config.network.dataPolicy = clampIntValue(config.network.dataPolicy, DATA_POLICY_ALWAYS_OFF, DATA_POLICY_ALWAYS_ON);

  config.led.brightness = clampIntValue(config.led.brightness, 1, 100);
  config.led.quietStartMinutes = clampIntValue(config.led.quietStartMinutes, 0, 1439);
  config.led.quietEndMinutes = clampIntValue(config.led.quietEndMinutes, 0, 1439);

  config.reporting.reportHour = clampIntValue(config.reporting.reportHour, 0, 23);
  config.sleep.timeout = clampIntValue(config.sleep.timeout, 60, 86400);
  config.sleep.mode = clampIntValue(config.sleep.mode, 0, 1);
  config.watchdog.timeout = clampIntValue(config.watchdog.timeout, 10, 300);
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

  DynamicJsonDocument doc(16384);
  bool loaded = readConfigDocument(CONFIG_PATH, doc);
  if (!loaded) {
    const char* recoveryPath = nullptr;
    if (readConfigDocument(CONFIG_BACKUP_PATH, doc)) {
      recoveryPath = CONFIG_BACKUP_PATH;
    } else if (readConfigDocument(CONFIG_TMP_PATH, doc)) {
      recoveryPath = CONFIG_TMP_PATH;
    }

    if (recoveryPath) {
      if (SPIFFS.exists(CONFIG_PATH)) {
        SPIFFS.remove(CONFIG_CORRUPT_PATH);
        if (!SPIFFS.rename(CONFIG_PATH, CONFIG_CORRUPT_PATH)) {
          Serial.println("无法保留损坏的配置文件");
        }
      }
      if (!SPIFFS.exists(CONFIG_PATH) && SPIFFS.rename(recoveryPath, CONFIG_PATH)) {
        Serial.println("已从事务文件恢复配置");
      } else if (SPIFFS.exists(CONFIG_PATH)) {
        Serial.println("配置已从事务文件加载，等待下次恢复");
      }
      loaded = true;
    }
  } else {
    SPIFFS.remove(CONFIG_TMP_PATH);
    SPIFFS.remove(CONFIG_BACKUP_PATH);
  }

  if (!loaded) {
    Serial.println("配置文件不存在或损坏，使用默认配置");
    if (SPIFFS.exists(CONFIG_PATH)) {
      SPIFFS.remove(CONFIG_CORRUPT_PATH);
      SPIFFS.rename(CONFIG_PATH, CONFIG_CORRUPT_PATH);
    }
    saveConfig();
    smsFilter.loadFromConfigStrings(config.smsFilter.whitelist, config.smsFilter.blockedKeywords);
    return;
  }

  assignIfPresent(doc.as<JsonVariantConst>(), "lang", config.lang);

  JsonVariantConst timeConfig = doc["time"];
  assignIfPresent(timeConfig, "timezoneOffsetMinutes", config.time.timezoneOffsetMinutes);

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

  JsonVariantConst led = doc["led"];
  assignIfPresent(led, "enabled", config.led.enabled);
  assignIfPresent(led, "brightness", config.led.brightness);
  assignIfPresent(led, "quietHoursEnabled", config.led.quietHoursEnabled);
  assignIfPresent(led, "quietStartMinutes", config.led.quietStartMinutes);
  assignIfPresent(led, "quietEndMinutes", config.led.quietEndMinutes);

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

  normalizeConfigValues();

  Serial.println("配置加载完成");
  smsFilter.loadFromConfigStrings(config.smsFilter.whitelist, config.smsFilter.blockedKeywords);
}

bool saveConfig() {
  if (!spiffsInitialized) {
    Serial.println("SPIFFS未初始化，无法保存配置");
    return false;
  }

  normalizeConfigValues();

  DynamicJsonDocument doc(16384);
  populateConfigDocument(doc, true, true);
  if (doc.overflowed()) {
    Serial.println("配置数据过大，无法保存");
    return false;
  }

  SPIFFS.remove(CONFIG_TMP_PATH);
  File file = SPIFFS.open(CONFIG_TMP_PATH, "w");
  if (!file) {
    Serial.println("无法创建配置文件");
    return false;
  }

  size_t bytesWritten = serializeJson(doc, file);
  file.flush();
  size_t fileSize = file.size();
  file.close();
  if (bytesWritten == 0 || fileSize == 0) {
    SPIFFS.remove(CONFIG_TMP_PATH);
    Serial.println("配置写入失败");
    return false;
  }

  SPIFFS.remove(CONFIG_BACKUP_PATH);
  bool hadExisting = SPIFFS.exists(CONFIG_PATH);
  if (hadExisting && !SPIFFS.rename(CONFIG_PATH, CONFIG_BACKUP_PATH)) {
    SPIFFS.remove(CONFIG_TMP_PATH);
    Serial.println("无法备份当前配置");
    return false;
  }

  if (!SPIFFS.rename(CONFIG_TMP_PATH, CONFIG_PATH)) {
    if (hadExisting) {
      SPIFFS.rename(CONFIG_BACKUP_PATH, CONFIG_PATH);
    }
    SPIFFS.remove(CONFIG_TMP_PATH);
    Serial.println("无法提交新配置");
    return false;
  }

  SPIFFS.remove(CONFIG_BACKUP_PATH);
  Serial.println("配置已保存");
  return true;
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

  config.time.timezoneOffsetMinutes = 480;

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

  config.led.enabled = true;
  config.led.brightness = 30;
  config.led.quietHoursEnabled = false;
  config.led.quietStartMinutes = 22 * 60;
  config.led.quietEndMinutes = 7 * 60;

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

  config.webAuth.enabled = true;
  config.webAuth.username = "admin";
  config.webAuth.password = "admin1234";
}
