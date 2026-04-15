#include "web_server.h"
#include <ArduinoJson.h>

#include "config_manager.h"
#include "battery_manager.h"
#include "log_manager.h"
#include "sim7670g_manager.h"
#include "web_pages_full.h"
#include "statistics_manager.h"
#include "notification_manager.h"
#include "sms_storage.h"
#include "wifi_manager.h"
#include "led_controller.h"
#include "sms_filter.h"
#include "watchdog_manager.h"
#include "i18n.h"
#include "operator_db.h"
#include "time_manager.h"

WebServer server(80);

#define AUTH_WRAP(handler) []() { if (!ensureAuthenticated()) return; handler(); }

inline void touchActivity() {
  sleepManager.updateActivity();
}

template <typename TDoc>
static void sendJsonDocument(int code, const TDoc& doc) {
  String payload;
  serializeJson(doc, payload);
  server.send(code, "application/json", payload);
}

static bool ensureAuthenticated() {
  if (!config.webAuth.enabled || config.webAuth.username.isEmpty() || config.webAuth.password.isEmpty()) {
    return true;
  }
  if (server.authenticate(config.webAuth.username.c_str(), config.webAuth.password.c_str())) {
    return true;
  }
  server.requestAuthentication(BASIC_AUTH, "SMS Forwarder");
  return false;
}

static String jsonError(const char* key) {
  DynamicJsonDocument doc(256);
  doc["success"] = false;
  doc["error"] = i18nGet(key);
  String response;
  serializeJson(doc, response);
  return response;
}

static String jsonMessage(const char* key) {
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["message"] = i18nGet(key);
  String response;
  serializeJson(doc, response);
  return response;
}

static String readJsonField(const char* key) {
  String body = server.arg("plain");
  if (body.isEmpty()) return "";
  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    return "";
  }
  JsonVariant value = doc[key];
  return value.isNull() ? "" : value.as<String>();
}

static bool readJsonBoolField(const char* key, bool& valueOut) {
  String body = server.arg("plain");
  if (body.isEmpty()) return false;
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    return false;
  }
  JsonVariant value = doc[key];
  if (value.isNull()) return false;
  valueOut = value.as<bool>();
  return true;
}

static String formatOperatorDisplay(const String& code, const String& fallback) {
  String name = getOperatorNameByCode(code, getCurrentLangCode());
  if (!name.isEmpty() && name != code) {
    return name;
  }
  if (!fallback.isEmpty() && fallback != "Unknown") {
    return fallback;
  }
  if (!code.isEmpty() && code != "Unknown") {
    return code;
  }
  return String(i18nGet("value_unknown"));
}

static String formatMaybeUnknown(const String& value) {
  if (value.isEmpty() || value == "Unknown") {
    return String(i18nGet("value_unknown"));
  }
  return value;
}

void initWebServer() {
  server.on("/", HTTP_GET, []() {
    if (!ensureAuthenticated()) return;
    server.send_P(200, "text/html", INDEX_HTML);
  });
  
  server.on("/api/status", HTTP_GET, AUTH_WRAP(handleGetStatus));
  server.on("/api/config", HTTP_GET, AUTH_WRAP(handleGetConfig));
  server.on("/api/battery", HTTP_GET, AUTH_WRAP(handleGetBattery));
  server.on("/api/system/version", HTTP_GET, AUTH_WRAP(handleGetVersion));
  server.on("/api/system/info", HTTP_GET, AUTH_WRAP(handleGetSystemInfo));
  server.on("/api/system/time", HTTP_GET, AUTH_WRAP(handleGetTimeStatus));
  server.on("/api/logs", HTTP_GET, AUTH_WRAP(handleGetLogs));
  server.on("/api/logs", HTTP_DELETE, AUTH_WRAP(handleClearLogs));
  server.on("/api/statistics", HTTP_GET, AUTH_WRAP(handleGetStatistics));
  server.on("/api/statistics", HTTP_DELETE, AUTH_WRAP(handleResetStatistics));
  server.on("/api/debug/restart", HTTP_POST, AUTH_WRAP(handleDebugRestart));
  server.on("/api/debug/system", HTTP_GET, AUTH_WRAP(handleDebugSystem));
  server.on("/api/debug/at", HTTP_POST, AUTH_WRAP(handleDebugAT));
  server.on("/api/debug/wifi", HTTP_POST, AUTH_WRAP(handleDebugWiFi));
  server.on("/api/debug/network", HTTP_POST, AUTH_WRAP(handleDebugNetwork));
  server.on("/api/debug/notification", HTTP_POST, AUTH_WRAP(handleDebugNotification));
  server.on("/api/debug/time", HTTP_POST, AUTH_WRAP(handleDebugTimeSync));
  server.on("/api/debug/echo", HTTP_POST, AUTH_WRAP(handleDebugEcho));
  server.on("/api/debug/led", HTTP_POST, AUTH_WRAP(handleDebugLED));
  server.on("/api/config/wifi", HTTP_POST, AUTH_WRAP(handleSetConfig));
  server.on("/api/config/lang", HTTP_POST, AUTH_WRAP(handleSetLanguage));
  server.on("/api/config/notification", HTTP_POST, AUTH_WRAP(handleSetNotificationConfig));
  server.on("/api/config/battery", HTTP_POST, AUTH_WRAP(handleSetBatteryConfig));
  server.on("/api/config/network", HTTP_POST, AUTH_WRAP(handleSetNetworkConfig));
  server.on("/api/config/smsfilter", HTTP_POST, AUTH_WRAP(handleSetSMSFilterConfig));
  server.on("/api/config/system", HTTP_POST, AUTH_WRAP(handleSetSystemConfig));
  server.on("/api/test/notification", HTTP_POST, AUTH_WRAP(handleTestNotification));
  server.on("/api/sim/reset", HTTP_POST, AUTH_WRAP(handleResetSIM));

  server.on("/api/sms", HTTP_GET, AUTH_WRAP(handleGetSMS));
  server.on("/api/sms", HTTP_DELETE, AUTH_WRAP(handleClearSMS));
  server.on("/api/sms/delete", HTTP_POST, AUTH_WRAP(handleDeleteSMS));
  server.on("/api/sms/forward", HTTP_POST, AUTH_WRAP(handleForwardSMS));
  server.on("/api/sms/send", HTTP_POST, AUTH_WRAP(handleSendSMS));
  server.on("/api/sms/check", HTTP_POST, AUTH_WRAP(handleCheckSMS));
  server.on("/api/forward-status", HTTP_GET, AUTH_WRAP(handleGetForwardStatus));
  server.on("/api/system/status", HTTP_GET, AUTH_WRAP(handleGetSystemStatus));
  server.on("/api/system/refresh", HTTP_POST, AUTH_WRAP(handleRefreshSystemStatus));
  
  server.begin();
  Serial.println("Web server started");
}

void handleGetStatus() {
  touchActivity();
  SystemStatus sysStatus = systemStatus.getStatus();
  BatteryInfo battery = getBatteryInfo();
  String operatorName = formatOperatorDisplay(sysStatus.operatorCode, sysStatus.operatorName);
  String homeOperatorName = formatOperatorDisplay(sysStatus.homeOperatorCode, sysStatus.homeOperatorName);
  String networkType = formatMaybeUnknown(sysStatus.networkType);

  DynamicJsonDocument doc(2048);
  doc["signal"] = sysStatus.signalStrength;
  doc["network"] = sysStatus.networkConnected ? "Connected" : "Disconnected";
  doc["simStatus"] = sysStatus.simReady ? "Ready" : "Not Ready";
  doc["operator"] = operatorName;
  doc["operatorCode"] = sysStatus.operatorCode;
  doc["homeOperator"] = homeOperatorName;
  doc["homeOperatorCode"] = sysStatus.homeOperatorCode;
  doc["networkType"] = networkType;
  doc["isRoaming"] = sysStatus.isRoaming;
  doc["smsAvailable"] = sysStatus.networkConnected;
  doc["csRegistered"] = sysStatus.csRegistered;
  doc["epsRegistered"] = sysStatus.epsRegistered;
  doc["dataAttached"] = sysStatus.dataAttached;
  doc["dataPolicy"] = config.network.dataPolicy;
  doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
  doc["wifiRssi"] = WiFi.RSSI();
  doc["wifiIp"] = WiFi.localIP().toString();
  doc["ledStatus"] = getLedStatus();
  doc["ledReason"] = getLedReason();
  doc["battery"] = battery.percentage;
  doc["batteryDisplay"] = battery.displayPercentage;
  doc["voltage"] = battery.voltage;
  doc["isCharging"] = battery.isCharging;
  doc["memory"] = ESP.getFreeHeap() / 1024;
  doc["timestamp"] = millis();
  sendJsonDocument(200, doc);
}

void handleGetConfig() {
  touchActivity();
  DynamicJsonDocument doc(16384);
  deserializeJson(doc, exportConfigAsJson(true, false));
  JsonObject wifi = doc["wifi"].as<JsonObject>();
  wifi["dns1Current"] = WiFi.dnsIP(0).toString();
  wifi["dns2Current"] = WiFi.dnsIP(1).toString();
  sendJsonDocument(200, doc);
}

void handleSetConfig() {
  touchActivity();
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  
  if (!ssid.isEmpty()) {
    config.wifi.ssid = ssid;
    config.wifi.password = password;
    config.wifi.useCustomDns = (server.arg("useCustomDns") == "true");
    config.wifi.forceStaticDns = (server.arg("forceStaticDns") == "true");
    config.wifi.staticIp = server.arg("staticIp");
    config.wifi.staticGateway = server.arg("staticGateway");
    config.wifi.staticSubnet = server.arg("staticSubnet");
    config.wifi.dns1 = server.arg("dns1");
    config.wifi.dns2 = server.arg("dns2");
    
    saveConfig();
    
    LOGI("WEB", "web_wifi_updated", ssid.c_str());
    server.send(200, "application/json", "{\"success\":true}");
    
    // Delay reconnect WiFi / 延迟重启连接WiFi
    delay(1000);
    WiFi.disconnect();
    delay(500);
    connectWiFi();
  } else {
    server.send(400, "application/json", jsonError("web_err_ssid_empty"));
  }
}

void handleGetBattery() {
  touchActivity();
  BatteryInfo battery = getBatteryInfo();
  DynamicJsonDocument doc(512);
  doc["voltage"] = battery.voltage;
  doc["percentage"] = battery.percentage;
  doc["displayPercentage"] = battery.displayPercentage;
  doc["isCharging"] = battery.isCharging;
  doc["isLowBattery"] = battery.isLowBattery;
  doc["chargeRate"] = battery.chargeRate;
  doc["timestamp"] = millis();
  sendJsonDocument(200, doc);
}

void handleDebugSystem() {
  touchActivity();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleDebugTimeSync() {
  touchActivity();
  LOGI("TIME", "time_sync_manual_start");

  bool ntpOk = false;
  bool modemOk = false;
  if (WiFi.status() == WL_CONNECTED) {
    ntpOk = initTimeSync();
  }
  if (!ntpOk) {
    modemOk = syncTimeFromModem();
  }
  bool ok = ntpOk || modemOk;
  String source = ntpOk ? "ntp" : (modemOk ? "modem" : "none");

  if (ok) {
    LOGI("TIME", "time_sync_manual_ok", source.c_str());
  } else {
    LOGW("TIME", "time_sync_manual_fail");
  }

  DynamicJsonDocument doc(256);
  doc["success"] = ok;
  doc["source"] = source;
  sendJsonDocument(200, doc);
}

void handleDebugRestart() {
  touchActivity();
  server.send(200, "application/json", "{\"success\":true}");
  delay(1000);
  ESP.restart();
}

void handleDebugAT() {
  touchActivity();
  String command = server.arg("command");
  if (command.isEmpty()) {
    command = readJsonField("command");
  }
  if (command.isEmpty()) {
    server.send(400, "application/json", "{\"error\":\"Missing command\"}");
    return;
  }
  
  DynamicJsonDocument doc(1536);
  doc["response"] = sendATCommand(command);
  sendJsonDocument(200, doc);
}

void handleDebugWiFi() {
  touchActivity();
  diagnoseWiFi();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetLanguage() {
  touchActivity();
  if (!server.hasArg("lang")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"missing_lang\"}");
    return;
  }
  String lang = server.arg("lang");
  lang.trim();
  lang.toLowerCase();
  if (lang.startsWith("zh")) {
    config.lang = "zh";
  } else if (lang.startsWith("en")) {
    config.lang = "en";
  } else {
    config.lang = "auto";
  }
  saveConfig();
  LOGI("WEB", "web_lang_updated", config.lang.c_str());
  server.send(200, "application/json", "{\"success\":true}");
}

void handleDebugNetwork() {
  touchActivity();
  String url = server.arg("url");
  String method = server.arg("method");
  String payload = server.arg("payload");
  diagnoseNetwork(url, method, payload);
  server.send(200, "application/json", "{\"success\":true}");
}

void handleDebugNotification() {
  touchActivity();
  handleTestNotification();
}

void handleGetVersion() {
  touchActivity();
  DynamicJsonDocument doc(256);
  doc["version"] = "1.0.0";
  doc["buildTime"] = String(__DATE__) + " " + String(__TIME__);
  doc["chipModel"] = ESP.getChipModel();
  sendJsonDocument(200, doc);
}

void handleGetStatistics() {
  touchActivity();
  Statistics stats = statisticsManager.getStatistics();
  DynamicJsonDocument doc(256);
  doc["totalSMSReceived"] = stats.totalSMSReceived;
  doc["totalSMSForwarded"] = stats.totalSMSForwarded;
  doc["totalSMSFiltered"] = stats.totalSMSFiltered;
  doc["totalPushSuccess"] = stats.totalPushSuccess;
  doc["totalPushFailed"] = stats.totalPushFailed;
  doc["totalRetries"] = stats.totalRetries;
  doc["uptime"] = stats.uptime;
  sendJsonDocument(200, doc);
}

void handleResetStatistics() {
  touchActivity();
  statisticsManager.resetStatistics();
  LOGI("WEB", "web_stats_reset");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleGetLogs() {
  touchActivity();
  String minLevel = server.arg("level");
  String filter = server.arg("filter");
  uint8_t level = minLevel.isEmpty() ? 0 : minLevel.toInt();
  size_t offset = 0;
  size_t limit = 100;

  if (server.hasArg("offset")) {
    long parsedOffset = server.arg("offset").toInt();
    if (parsedOffset > 0) {
      offset = static_cast<size_t>(parsedOffset);
    }
  }
  if (server.hasArg("limit")) {
    long parsedLimit = server.arg("limit").toInt();
    if (parsedLimit > 0) {
      limit = static_cast<size_t>(parsedLimit);
    }
  }
  
  String response = logManager.getLogsAsJson(level, filter, offset, limit);
  server.send(200, "application/json", response);
}

void handleClearLogs() {
  touchActivity();
  logManager.clearLogs();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetNotificationConfig() {
  touchActivity();
  LOGI("WEB", "web_notify_update_start");
  
  // Debug: log all params / 调试：打印所有参数
  for (int i = 0; i < server.args(); i++) {
    LOGD("WEB", "web_param", server.argName(i).c_str(), server.arg(i).c_str());
  }
  
  // Bark config / Bark配置
  if (server.hasArg("barkKey")) config.bark.key = server.arg("barkKey");
  if (server.hasArg("barkUrl")) config.bark.url = server.arg("barkUrl");
  
  // ServerChan config / Server酱配置
  if (server.hasArg("serverChanKey")) config.serverChan.key = server.arg("serverChanKey");
  if (server.hasArg("serverChanUrl")) config.serverChan.url = server.arg("serverChanUrl");
  
  // Telegram config / Telegram配置
  if (server.hasArg("telegramToken")) config.telegram.token = server.arg("telegramToken");
  if (server.hasArg("telegramChatId")) config.telegram.chatId = server.arg("telegramChatId");
  if (server.hasArg("telegramUrl")) config.telegram.url = server.arg("telegramUrl");
  
  // DingTalk config / 钉钉配置
  if (server.hasArg("dingtalkWebhook")) config.dingtalk.webhook = server.arg("dingtalkWebhook");
  
  // Feishu config / 飞书配置
  if (server.hasArg("feishuWebhook")) config.feishu.webhook = server.arg("feishuWebhook");
  
  // Custom config / 自定义配置
  if (server.hasArg("customUrl")) config.custom.url = server.arg("customUrl");
  if (server.hasArg("customKey")) config.custom.key = server.arg("customKey");

  bool hasToggle =
      server.hasArg("bark-enabled") ||
      server.hasArg("serverchan-enabled") ||
      server.hasArg("telegram-enabled") ||
      server.hasArg("dingtalk-enabled") ||
      server.hasArg("feishu-enabled") ||
      server.hasArg("custom-enabled");

  if (hasToggle) {
    config.bark.enabled = server.hasArg("bark-enabled") && !config.bark.key.isEmpty();
    config.serverChan.enabled = server.hasArg("serverchan-enabled") && !config.serverChan.key.isEmpty();
    config.telegram.enabled = server.hasArg("telegram-enabled") && !config.telegram.token.isEmpty();
    config.dingtalk.enabled = server.hasArg("dingtalk-enabled") && !config.dingtalk.webhook.isEmpty();
    config.feishu.enabled = server.hasArg("feishu-enabled") && !config.feishu.webhook.isEmpty();
    config.custom.enabled = server.hasArg("custom-enabled") && !config.custom.url.isEmpty();
  } else {
    config.bark.enabled = !config.bark.key.isEmpty();
    config.serverChan.enabled = !config.serverChan.key.isEmpty();
    config.telegram.enabled = !config.telegram.token.isEmpty();
    config.dingtalk.enabled = !config.dingtalk.webhook.isEmpty();
    config.feishu.enabled = !config.feishu.webhook.isEmpty();
    config.custom.enabled = !config.custom.url.isEmpty();
  }
  
  LOGI("WEB", "web_bark_config", config.bark.enabled ? i18nGet("bool_true") : i18nGet("bool_false"), config.bark.key.c_str());
  
  // Save config / 保存配置
  saveConfig();
  
  LOGI("WEB", "web_notify_updated");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleTestNotification() {
  touchActivity();
  LOGI("TEST", "web_notify_test_start");
  String testTitle = i18nGet("web_test_title");
  String testMessage = server.arg("message");
  if (testMessage.isEmpty()) {
    testMessage = readJsonField("message");
  }
  if (testMessage.isEmpty()) {
    testMessage = i18nFormat("web_test_message", String(millis()).c_str());
  }
  
  DynamicJsonDocument doc(768);
  JsonObject results = doc.createNestedObject("results");
  int totalTests = 0;
  int successCount = 0;
  
  if (config.bark.enabled && !config.bark.key.isEmpty()) {
    totalTests++;
    LOGI("TEST", "web_bark_test", config.bark.key.c_str(), config.bark.url.c_str());
    bool success = notificationManager.sendToBark(testTitle, testMessage);
    results["bark"] = success;
    if (success) successCount++;
  } else {
    LOGI("TEST", "web_bark_not_enabled", config.bark.enabled ? i18nGet("bool_true") : i18nGet("bool_false"), config.bark.key.c_str());
  }
  
  if (config.serverChan.enabled && !config.serverChan.key.isEmpty()) {
    totalTests++;
    bool success = notificationManager.sendToServerChan(testTitle, testMessage);
    results["serverChan"] = success;
    if (success) successCount++;
  }
  
  if (config.telegram.enabled && !config.telegram.token.isEmpty()) {
    totalTests++;
    bool success = notificationManager.sendToTelegram(testTitle, testMessage);
    results["telegram"] = success;
    if (success) successCount++;
  }

  doc["total"] = totalTests;
  doc["success"] = successCount;
  sendJsonDocument(200, doc);
}

void handleSetBatteryConfig() {
  touchActivity();
  if (server.hasArg("lowThreshold")) config.battery.lowThreshold = server.arg("lowThreshold").toInt();
  if (server.hasArg("criticalThreshold")) config.battery.criticalThreshold = server.arg("criticalThreshold").toInt();
  config.battery.alertEnabled = server.hasArg("battery-alert-enabled");
  config.battery.lowBatteryAlertEnabled = server.hasArg("low-battery-alert-enabled");
  config.battery.chargingAlertEnabled = server.hasArg("charging-alert-enabled");
  config.battery.fullChargeAlertEnabled = server.hasArg("full-charge-alert-enabled");
  
  saveConfig();
  LOGI("WEB", "web_battery_updated");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetNetworkConfig() {
  touchActivity();
  config.network.roamingAlertEnabled = server.hasArg("roaming-alert-enabled");
  config.network.autoDisableDataRoaming = server.hasArg("auto-disable-data-roaming");
  config.network.allowSmsDataRoaming = server.hasArg("allow-sms-data-roaming");
  if (server.hasArg("signalCheckInterval")) config.network.signalCheckInterval = server.arg("signalCheckInterval").toInt();
  if (server.hasArg("operatorMode")) config.network.operatorMode = server.arg("operatorMode").toInt();
  if (server.hasArg("radioMode")) config.network.radioMode = server.arg("radioMode").toInt();
  if (server.hasArg("dataPolicy")) config.network.dataPolicy = server.arg("dataPolicy").toInt();
  if (server.hasArg("apn")) config.network.apn = server.arg("apn");
  if (server.hasArg("apnUser")) config.network.apnUser = server.arg("apnUser");
  if (server.hasArg("apnPass")) config.network.apnPass = server.arg("apnPass");
  
  saveConfig();
  LOGI("WEB", "web_network_updated");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetSMSFilterConfig() {
  touchActivity();
  config.smsFilter.whitelistEnabled = server.hasArg("whitelist-enabled");
  config.smsFilter.keywordFilterEnabled = server.hasArg("keyword-filter-enabled");
  if (server.hasArg("whitelist")) config.smsFilter.whitelist = server.arg("whitelist");
  if (server.hasArg("blockedKeywords")) config.smsFilter.blockedKeywords = server.arg("blockedKeywords");
  
  smsFilter.loadFromConfigStrings(config.smsFilter.whitelist, config.smsFilter.blockedKeywords);
  
  saveConfig();
  LOGI("WEB", "web_smsfilter_updated");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetSystemConfig() {
  touchActivity();
  config.reporting.dailyReportEnabled = server.hasArg("daily-report-enabled");
  config.reporting.weeklyReportEnabled = server.hasArg("weekly-report-enabled");
  if (server.hasArg("reportHour")) config.reporting.reportHour = server.arg("reportHour").toInt();
  
  // Debug config / 调试配置
  config.debug.atCommandEcho = server.hasArg("at-command-echo");
  
  // Sleep config / 休眠配置
  config.sleep.enabled = server.hasArg("sleep-enabled");
  if (server.hasArg("sleep-timeout")) config.sleep.timeout = server.arg("sleep-timeout").toInt();
  if (server.hasArg("sleep-mode")) config.sleep.mode = server.arg("sleep-mode").toInt();
  sleepManager.configure(config.sleep.enabled, config.sleep.timeout, config.sleep.mode);

  // Watchdog config / 看门狗配置
  if (server.hasArg("wdt-timeout")) {
    config.watchdog.timeout = server.arg("wdt-timeout").toInt();
  }

  String webAuthUsername = server.arg("web-auth-username");
  String webAuthPassword = server.arg("web-auth-password");
  webAuthUsername.trim();
  webAuthPassword.trim();
  bool webAuthEnabled = server.hasArg("web-auth-enabled");
  if (webAuthEnabled && webAuthUsername.isEmpty()) {
    server.send(400, "application/json", jsonError("web_err_webauth_username"));
    return;
  }
  if (webAuthEnabled && config.webAuth.password.isEmpty() && webAuthPassword.isEmpty()) {
    server.send(400, "application/json", jsonError("web_err_webauth_password"));
    return;
  }
  config.webAuth.enabled = webAuthEnabled;
  if (!webAuthUsername.isEmpty()) {
    config.webAuth.username = webAuthUsername;
  }
  if (!webAuthPassword.isEmpty()) {
    config.webAuth.password = webAuthPassword;
  }
  
  saveConfig();
  LOGI("WEB", "web_system_updated");
  watchdogManager.disableWatchdog();
  watchdogManager.initWatchdog();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleGetSystemInfo() {
  touchActivity();
  static String cachedInfo = "";
  
  if (cachedInfo.isEmpty()) {
    DynamicJsonDocument doc(512);
    doc["totalMemory"] = ESP.getHeapSize() / 1024;
    doc["cpuFreq"] = ESP.getCpuFreqMHz();
    doc["flashSize"] = ESP.getFlashChipSize() / (1024 * 1024);
    doc["chipModel"] = ESP.getChipModel();
    doc["chipRevision"] = ESP.getChipRevision();
    doc["chipCores"] = ESP.getChipCores();
    serializeJson(doc, cachedInfo);
  }
  
  server.send(200, "application/json", cachedInfo);
}

void handleGetTimeStatus() {
  touchActivity();
  uint64_t epochMs = getEpochMillis();
  bool synced = isTimeSynced();
  String source = getTimeSyncSource();
  DynamicJsonDocument doc(256);
  doc["synced"] = synced;
  char epochBuf[24];
  snprintf(epochBuf, sizeof(epochBuf), "%llu", static_cast<unsigned long long>(epochMs));
  doc["epochMs"] = serialized(epochBuf);
  doc["source"] = source;
  sendJsonDocument(200, doc);
}

void handleGetSMS() {
  touchActivity();
  Statistics stats = statisticsManager.getStatistics();
  DynamicJsonDocument doc(32768);
  JsonObject statsJson = doc.createNestedObject("stats");
  statsJson["received"] = stats.totalSMSReceived;
  statsJson["forwarded"] = stats.totalSMSForwarded;
  statsJson["filtered"] = stats.totalSMSFiltered;
  statsJson["stored"] = smsStorage.getSMSCount();
  JsonArray messages = doc.createNestedArray("messages");

  std::vector<SMSRecord> records = smsStorage.getAllSMS();
  for (const auto& record : records) {
    JsonObject message = messages.createNestedObject();
    message["id"] = record.id;
    message["sender"] = record.sender;
    message["content"] = record.content;
    message["timestamp"] = record.timestamp;
    message["status"] = record.status;
    message["forwarded"] = SMSStorage::isSuccessStatus(record.status);
    message["retryCount"] = record.retryCount;
    message["lastAttemptAt"] = record.lastAttemptAt;
    message["lastError"] = record.lastError;
    message["canManualForward"] = SMSStorage::canManualForward(record.status);
  }

  sendJsonDocument(200, doc);
}

void handleClearSMS() {
  touchActivity();
  smsStorage.clearAllSMS();
  LOGI("WEB", "web_sms_cleared");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleDeleteSMS() {
  touchActivity();
  String id = server.arg("id");
  if (id.isEmpty()) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing ID\"}");
    return;
  }
  
  int smsId = id.toInt();
  if (smsId <= 0) {
    server.send(400, "application/json", jsonError("web_err_sms_id_invalid"));
    return;
  }
  
  bool removed = smsStorage.deleteSMS(smsId);
  if (removed) {
    LOGI("WEB", "web_sms_deleted", id.c_str());
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(404, "application/json", jsonError("web_err_sms_not_found"));
  }
}

void handleForwardSMS() {
  touchActivity();
  String id = server.arg("id");
  if (id.isEmpty()) {
    server.send(400, "application/json", jsonError("web_err_missing_id"));
    return;
  }
  
  // Load SMS content / 获取短信内容
  SMSRecord sms = smsStorage.getSMSById(id.toInt());
  if (sms.id == 0) {
    server.send(404, "application/json", jsonError("web_err_sms_not_found"));
    return;
  }
  if (!SMSStorage::canManualForward(sms.status)) {
    server.send(400, "application/json", jsonError("web_sms_forward_fail"));
    return;
  }
  
  LOGI("WEB", "web_sms_forward_manual", id.c_str(), sms.sender.c_str());
  int smsId = id.toInt();
  smsStorage.updateSMSStatus(smsId, SMSStatus::PENDING_FORWARD, getTimestampMsString(), "", sms.retryCount);
  bool success = notificationManager.forwardSMS(sms.sender, sms.content, false, smsId, true);
  if (success) {
    statisticsManager.incrementSMSForwarded();
    LOGI("WEB", "web_sms_forward_success", id.c_str());
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", jsonError("web_sms_forward_fail"));
  }
}

void handleResetSIM() {
  touchActivity();
  resetSIMCheck();
  server.send(200, "application/json", jsonMessage("web_sim_reset"));
}

void handleGetForwardStatus() {
  touchActivity();
  Statistics stats = statisticsManager.getStatistics();
  DynamicJsonDocument doc(1024);
  JsonArray platforms = doc.createNestedArray("platforms");

  if (config.bark.enabled) {
    JsonObject item = platforms.createNestedObject();
    item["name"] = "Bark";
    item["enabled"] = true;
  }
  
  if (config.serverChan.enabled) {
    JsonObject item = platforms.createNestedObject();
    item["name"] = i18nGet("web_channel_serverchan");
    item["enabled"] = true;
  }
  
  if (config.telegram.enabled) {
    JsonObject item = platforms.createNestedObject();
    item["name"] = "Telegram";
    item["enabled"] = true;
  }

  doc["totalSuccess"] = stats.totalPushSuccess;
  doc["totalFailed"] = stats.totalPushFailed;
  sendJsonDocument(200, doc);
}

void handleGetSystemStatus() {
  touchActivity();
  SystemStatus sysStatus = systemStatus.getStatus();
  BatteryInfo battery = getBatteryInfo();
  String operatorName = formatOperatorDisplay(sysStatus.operatorCode, sysStatus.operatorName);
  String networkType = formatMaybeUnknown(sysStatus.networkType);

  DynamicJsonDocument doc(1024);
  doc["signal"] = sysStatus.signalStrength;
  doc["simReady"] = sysStatus.simReady;
  doc["networkConnected"] = sysStatus.networkConnected;
  doc["csRegistered"] = sysStatus.csRegistered;
  doc["epsRegistered"] = sysStatus.epsRegistered;
  doc["dataAttached"] = sysStatus.dataAttached;
  doc["dataPolicy"] = config.network.dataPolicy;
  doc["operatorName"] = operatorName;
  doc["networkType"] = networkType;
  doc["isRoaming"] = sysStatus.isRoaming;
  doc["battery"] = battery.percentage;
  doc["batteryDisplay"] = battery.displayPercentage;
  doc["voltage"] = battery.voltage;
  doc["isCharging"] = battery.isCharging;
  doc["memory"] = ESP.getFreeHeap() / 1024;
  doc["uptime"] = millis() / 1000;
  doc["lastUpdate"] = sysStatus.lastUpdate;
  sendJsonDocument(200, doc);
}

void handleRefreshSystemStatus() {
  touchActivity();
  String type = server.arg("type");
  
  if (type == "signal") {
    systemStatus.refreshSignalOnly();
    server.send(200, "application/json", jsonMessage("web_signal_refreshed"));
  } else if (type == "all") {
    systemStatus.refreshAllStatus();
    server.send(200, "application/json", jsonMessage("web_status_refreshed"));
  } else {
    server.send(400, "application/json", jsonError("web_refresh_invalid"));
  }
}

void handleSendSMS() {
  touchActivity();
  String phoneNumber = server.arg("phoneNumber");
  String message = server.arg("message");
  
  if (phoneNumber.isEmpty() || message.isEmpty()) {
    server.send(400, "application/json", jsonError("web_sms_send_required"));
    return;
  }
  
  if (simState != SIM_STATE_READY) {
    server.send(400, "application/json", jsonError("web_sim_not_ready"));
    return;
  }
  
  bool success = sendSMS(phoneNumber, message);
  
  if (success) {
    server.send(200, "application/json", jsonMessage("web_sms_send_ok"));
  } else {
    server.send(500, "application/json", jsonError("web_sms_send_fail"));
  }
}

void handleDebugEcho() {
  touchActivity();
  bool enabled = false;
  if (!readJsonBoolField("enabled", enabled)) {
    server.send(400, "application/json", jsonError("web_missing_enabled"));
    return;
  }
  config.debug.atCommandEcho = enabled;
  saveConfig();

  LOGI("WEB", enabled ? "web_at_echo_on" : "web_at_echo_off");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleDebugLED() {
  touchActivity();
  String test = server.arg("test");
  
  if (test == "hardware") {
    testLEDHardware();
    server.send(200, "application/json", jsonMessage("web_led_hw_done"));
  } else if (test == "states") {
    testAllLEDStates();
    server.send(200, "application/json", jsonMessage("web_led_state_done"));
  } else {
    server.send(400, "application/json", jsonError("web_led_test_invalid"));
  }
}

void handleCheckSMS() {
  touchActivity();
  if (simState != SIM_STATE_READY) {
    server.send(400, "application/json", jsonError("web_sim_not_ready"));
    return;
  }
  
  LOGI("WEB", "web_sms_check");
  checkAllSMS();
  
  server.send(200, "application/json", jsonMessage("web_sms_check_started"));
}
