#include "config_manager.h"
#include <SPIFFS.h>

Config config;

void initConfig() {
  // 初始化SPIFFS，强制格式化
  Serial.println("初始化SPIFFS...");
  if (!SPIFFS.begin(false)) {
    Serial.println("SPIFFS首次初始化失败，尝试格式化...");
    if (!SPIFFS.begin(true)) {
      Serial.println("SPIFFS格式化失败，使用SD卡存储");
    } else {
      Serial.println("SPIFFS格式化成功");
    }
  } else {
    Serial.println("SPIFFS初始化成功");
  }
  
  loadConfig();
}

void loadConfig() {
  File file;
  
  // 使用SPIFFS加载配置
  if (SPIFFS.begin(true)) {
    file = SPIFFS.open("/config.json", "r");
  }
  
  if (!file) {
    Serial.println("配置文件不存在，使用默认配置");
    setDefaultConfig();
    saveConfig();
    return;
  }

  String content = file.readString();
  file.close();

  // 解析嵌套JSON结构
  String wifiSection = extractSection(content, "\"wifi\":");
  if (!wifiSection.isEmpty()) {
    parseConfigValue(wifiSection, "\"ssid\":\"", config.wifi.ssid);
    parseConfigValue(wifiSection, "\"password\":\"", config.wifi.password);
  }
  
  String barkSection = extractSection(content, "\"bark\":");
  if (!barkSection.isEmpty()) {
    parseConfigBool(barkSection, "\"enabled\":", config.bark.enabled);
    parseConfigValue(barkSection, "\"key\":\"", config.bark.key);
    parseConfigValue(barkSection, "\"url\":\"", config.bark.url);
  }
  
  String serverChanSection = extractSection(content, "\"serverChan\":");
  if (!serverChanSection.isEmpty()) {
    parseConfigBool(serverChanSection, "\"enabled\":", config.serverChan.enabled);
    parseConfigValue(serverChanSection, "\"key\":\"", config.serverChan.key);
    parseConfigValue(serverChanSection, "\"url\":\"", config.serverChan.url);
  }
  
  String telegramSection = extractSection(content, "\"telegram\":");
  if (!telegramSection.isEmpty()) {
    parseConfigBool(telegramSection, "\"enabled\":", config.telegram.enabled);
    parseConfigValue(telegramSection, "\"token\":\"", config.telegram.token);
    parseConfigValue(telegramSection, "\"chatId\":\"", config.telegram.chatId);
    parseConfigValue(telegramSection, "\"url\":\"", config.telegram.url);
  }
  
  String dingtalkSection = extractSection(content, "\"dingtalk\":");
  if (!dingtalkSection.isEmpty()) {
    parseConfigBool(dingtalkSection, "\"enabled\":", config.dingtalk.enabled);
    parseConfigValue(dingtalkSection, "\"webhook\":\"", config.dingtalk.webhook);
  }
  
  String feishuSection = extractSection(content, "\"feishu\":");
  if (!feishuSection.isEmpty()) {
    parseConfigBool(feishuSection, "\"enabled\":", config.feishu.enabled);
    parseConfigValue(feishuSection, "\"webhook\":\"", config.feishu.webhook);
  }
  
  String customSection = extractSection(content, "\"custom\":");
  if (!customSection.isEmpty()) {
    parseConfigBool(customSection, "\"enabled\":", config.custom.enabled);
    parseConfigValue(customSection, "\"url\":\"", config.custom.url);
    parseConfigValue(customSection, "\"key\":\"", config.custom.key);
  }
  
  String smsFilterSection = extractSection(content, "\"smsFilter\":");
  if (!smsFilterSection.isEmpty()) {
    parseConfigBool(smsFilterSection, "\"whitelistEnabled\":", config.smsFilter.whitelistEnabled);
    parseConfigBool(smsFilterSection, "\"keywordFilterEnabled\":", config.smsFilter.keywordFilterEnabled);
    parseConfigValue(smsFilterSection, "\"whitelist\":\"", config.smsFilter.whitelist);
    parseConfigValue(smsFilterSection, "\"blockedKeywords\":\"", config.smsFilter.blockedKeywords);
  }
  
  String reportingSection = extractSection(content, "\"reporting\":");
  if (!reportingSection.isEmpty()) {
    parseConfigBool(reportingSection, "\"dailyReportEnabled\":", config.reporting.dailyReportEnabled);
    parseConfigBool(reportingSection, "\"weeklyReportEnabled\":", config.reporting.weeklyReportEnabled);
    parseConfigInt(reportingSection, "\"reportHour\":", config.reporting.reportHour);
  }
  
  String batterySection = extractSection(content, "\"battery\":");
  if (!batterySection.isEmpty()) {
    parseConfigInt(batterySection, "\"lowThreshold\":", config.battery.lowThreshold);
    parseConfigInt(batterySection, "\"criticalThreshold\":", config.battery.criticalThreshold);
    parseConfigBool(batterySection, "\"alertEnabled\":", config.battery.alertEnabled);
    parseConfigBool(batterySection, "\"lowBatteryAlertEnabled\":", config.battery.lowBatteryAlertEnabled);
  }
  
  String networkSection = extractSection(content, "\"network\":");
  if (!networkSection.isEmpty()) {
    parseConfigBool(networkSection, "\"roamingAlertEnabled\":", config.network.roamingAlertEnabled);
    parseConfigBool(networkSection, "\"autoDisableDataRoaming\":", config.network.autoDisableDataRoaming);
    parseConfigInt(networkSection, "\"signalCheckInterval\":", config.network.signalCheckInterval);
    parseConfigInt(networkSection, "\"operatorMode\":", config.network.operatorMode);
    parseConfigValue(networkSection, "\"apn\":\"", config.network.apn);
    parseConfigValue(networkSection, "\"apnUser\":\"", config.network.apnUser);
    parseConfigValue(networkSection, "\"apnPass\":\"", config.network.apnPass);
  }
  
  String debugSection = extractSection(content, "\"debug\":");
  if (!debugSection.isEmpty()) {
    parseConfigBool(debugSection, "\"atCommandEcho\":", config.debug.atCommandEcho);
  }
  
  Serial.println("配置加载完成");
}

void parseConfigValue(const String& json, const String& key, String& value) {
  int pos = json.indexOf(key);
  if (pos >= 0) {
    int start = pos + key.length();
    int end = json.indexOf("\"", start);
    if (end > start) {
      value = json.substring(start, end);
    }
  }
}

void parseConfigBool(const String& json, const String& key, bool& value) {
  int pos = json.indexOf(key);
  if (pos >= 0) {
    int truePos = json.indexOf("true", pos);
    int falsePos = json.indexOf("false", pos);
    if (truePos >= 0 && (falsePos < 0 || truePos < falsePos)) {
      value = true;
    } else if (falsePos >= 0) {
      value = false;
    }
  }
}

void parseConfigInt(const String& json, const String& key, int& value) {
  int pos = json.indexOf(key);
  if (pos >= 0) {
    int start = pos + key.length();
    int end = json.indexOf(",", start);
    if (end < 0) end = json.indexOf("}", start);
    if (end > start) {
      String numStr = json.substring(start, end);
      numStr.trim();
      value = numStr.toInt();
    }
  }
}

String extractSection(const String& json, const String& sectionKey) {
  int start = json.indexOf(sectionKey);
  if (start < 0) return "";
  
  start = json.indexOf("{", start);
  if (start < 0) return "";
  
  int braceCount = 1;
  int pos = start + 1;
  
  while (pos < json.length() && braceCount > 0) {
    if (json.charAt(pos) == '{') braceCount++;
    else if (json.charAt(pos) == '}') braceCount--;
    pos++;
  }
  
  return json.substring(start, pos);
}

void saveConfig() {
  File file;
  
  // 使用SPIFFS保存配置
  if (SPIFFS.begin(true)) {
    file = SPIFFS.open("/config.json", "w");
  }
  
  if (!file) {
    Serial.println("无法创建配置文件");
    return;
  }

  // 完整的JSON配置
  String json = "{";
  json += "\"wifi\":{";
  json += "\"ssid\":\"" + config.wifi.ssid + "\",";
  json += "\"password\":\"" + config.wifi.password + "\"";
  json += "},";
  json += "\"bark\":{";
  json += "\"enabled\":" + String(config.bark.enabled ? "true" : "false") + ",";
  json += "\"key\":\"" + config.bark.key + "\",";
  json += "\"url\":\"" + config.bark.url + "\"";
  json += "},";
  json += "\"serverChan\":{";
  json += "\"enabled\":" + String(config.serverChan.enabled ? "true" : "false") + ",";
  json += "\"key\":\"" + config.serverChan.key + "\",";
  json += "\"url\":\"" + config.serverChan.url + "\"";
  json += "},";
  json += "\"telegram\":{";
  json += "\"enabled\":" + String(config.telegram.enabled ? "true" : "false") + ",";
  json += "\"token\":\"" + config.telegram.token + "\",";
  json += "\"chatId\":\"" + config.telegram.chatId + "\",";
  json += "\"url\":\"" + config.telegram.url + "\"";
  json += "},";
  json += "\"dingtalk\":{";
  json += "\"enabled\":" + String(config.dingtalk.enabled ? "true" : "false") + ",";
  json += "\"webhook\":\"" + config.dingtalk.webhook + "\"";
  json += "},";
  json += "\"feishu\":{";
  json += "\"enabled\":" + String(config.feishu.enabled ? "true" : "false") + ",";
  json += "\"webhook\":\"" + config.feishu.webhook + "\"";
  json += "},";
  json += "\"custom\":{";
  json += "\"enabled\":" + String(config.custom.enabled ? "true" : "false") + ",";
  json += "\"url\":\"" + config.custom.url + "\",";
  json += "\"key\":\"" + config.custom.key + "\"";
  json += "},";
  json += "\"battery\":{";
  json += "\"lowThreshold\":" + String(config.battery.lowThreshold) + ",";
  json += "\"criticalThreshold\":" + String(config.battery.criticalThreshold) + ",";
  json += "\"alertEnabled\":" + String(config.battery.alertEnabled ? "true" : "false") + ",";
  json += "\"lowBatteryAlertEnabled\":" + String(config.battery.lowBatteryAlertEnabled ? "true" : "false");
  json += "},";
  json += "\"smsFilter\":{";
  json += "\"whitelistEnabled\":" + String(config.smsFilter.whitelistEnabled ? "true" : "false") + ",";
  json += "\"keywordFilterEnabled\":" + String(config.smsFilter.keywordFilterEnabled ? "true" : "false") + ",";
  json += "\"whitelist\":\"" + config.smsFilter.whitelist + "\",";
  json += "\"blockedKeywords\":\"" + config.smsFilter.blockedKeywords + "\"";
  json += "},";
  json += "\"reporting\":{";
  json += "\"dailyReportEnabled\":" + String(config.reporting.dailyReportEnabled ? "true" : "false") + ",";
  json += "\"weeklyReportEnabled\":" + String(config.reporting.weeklyReportEnabled ? "true" : "false") + ",";
  json += "\"reportHour\":" + String(config.reporting.reportHour);
  json += "},";
  json += "\"network\":{";
  json += "\"roamingAlertEnabled\":" + String(config.network.roamingAlertEnabled ? "true" : "false") + ",";
  json += "\"autoDisableDataRoaming\":" + String(config.network.autoDisableDataRoaming ? "true" : "false") + ",";
  json += "\"signalCheckInterval\":" + String(config.network.signalCheckInterval) + ",";
  json += "\"operatorMode\":" + String(config.network.operatorMode) + ",";
  json += "\"apn\":\"" + config.network.apn + "\",";
  json += "\"apnUser\":\"" + config.network.apnUser + "\",";
  json += "\"apnPass\":\"" + config.network.apnPass + "\"";
  json += "},";
  json += "\"debug\":{";
  json += "\"atCommandEcho\":" + String(config.debug.atCommandEcho ? "true" : "false");
  json += "}";
  json += "}";

  file.print(json);
  file.close();
  Serial.println("配置已保存");
}

void setDefaultConfig() {
  config.wifi.ssid = "SMS-Forwarder";
  config.wifi.password = "12345678";
  config.bark.enabled = false;
  config.bark.key = "";
  config.bark.url = "https://api.day.app";
  config.serverChan.enabled = false;
  config.serverChan.key = "";
  config.serverChan.url = "https://sctapi.ftqq.com";
  config.telegram.enabled = false;
  config.telegram.token = "";
  config.telegram.chatId = "";
  config.dingtalk.enabled = false;
  config.dingtalk.webhook = "";
  config.feishu.enabled = false;
  config.feishu.webhook = "";
  config.custom.enabled = false;
  config.custom.url = "";
  config.custom.key = "";

  config.smsFilter.whitelistEnabled = false;
  config.smsFilter.keywordFilterEnabled = false;
  config.smsFilter.whitelist = "";
  config.smsFilter.blockedKeywords = "";
  
  // 网络漫游配置
  config.network.roamingAlertEnabled = true;
  config.network.autoDisableDataRoaming = true;
  config.network.signalCheckInterval = 30;
  config.network.operatorMode = 0;  // 自动选网
  config.network.apn = "CMNET";     // 中国移动默认APN
  config.network.apnUser = "";
  config.network.apnPass = "";
  
  // 电池配置
  config.battery.lowThreshold = 15;
  config.battery.criticalThreshold = 5;
  config.battery.alertEnabled = true;
  config.battery.chargingAlertEnabled = false;
  config.battery.lowBatteryAlertEnabled = true;
  config.battery.fullChargeAlertEnabled = false;
  
  // 休眠配置
  config.sleep.enabled = true;
  config.sleep.timeout = 1800; // 30分钟
  config.sleep.mode = 1; // Deep Sleep
  
  // 报告配置
  config.reporting.dailyReportEnabled = false;
  config.reporting.weeklyReportEnabled = false;
  config.reporting.reportHour = 9;
  
  // 调试配置
  config.debug.atCommandEcho = false;
}