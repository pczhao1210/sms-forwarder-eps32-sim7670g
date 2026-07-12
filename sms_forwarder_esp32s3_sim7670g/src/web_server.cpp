#include "web_server.h"
#include <ArduinoJson.h>

#include "config_manager.h"
#include "battery_manager.h"
#include "log_manager.h"
#include "sim7670g_manager.h"
#include "web_pages_full.h"
#include "statistics_manager.h"
#include "notification_manager.h"
#include "retry_manager.h"
#include "sms_storage.h"
#include "wifi_manager.h"
#include "led_controller.h"
#include "sms_filter.h"
#include "watchdog_manager.h"
#include "i18n.h"
#include "operator_db.h"
#include "time_manager.h"
#include <stdlib.h>

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

static void appendJsonEscaped(String& out, const String& value) {
  out += '"';
  for (int i = 0; i < value.length(); i++) {
    unsigned char c = static_cast<unsigned char>(value.charAt(i));
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (c < 32) {
          char escaped[7];
          snprintf(escaped, sizeof(escaped), "\\u%04x", c);
          out += escaped;
        } else {
          out += static_cast<char>(c);
        }
        break;
    }
  }
  out += '"';
}

static void appendJsonStringMember(String& out, const char* key, const String& value) {
  out += ",\"";
  out += key;
  out += "\":";
  appendJsonEscaped(out, value);
}

static size_t readSizeArgOrDefault(const char* field, size_t defaultValue, size_t maxValue) {
  if (!server.hasArg(field)) return defaultValue;
  String raw = server.arg(field);
  raw.trim();
  if (raw.isEmpty()) return defaultValue;

  char* endPtr = nullptr;
  unsigned long parsed = strtoul(raw.c_str(), &endPtr, 10);
  if (endPtr == raw.c_str() || *endPtr != '\0') return defaultValue;
  if (parsed > maxValue) return maxValue;
  return static_cast<size_t>(parsed);
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

static bool saveConfigOrSendError() {
  if (saveConfig()) return true;
  loadConfig();
  server.send(500, "application/json", "{\"success\":false,\"error\":\"config_save_failed\"}");
  return false;
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

static void sendRangeError(const char* field, int minValue, int maxValue) {
  DynamicJsonDocument doc(256);
  doc["success"] = false;
  doc["error"] = "invalid_range";
  doc["field"] = field;
  doc["min"] = minValue;
  doc["max"] = maxValue;
  sendJsonDocument(400, doc);
}

static void sendAllowedValueError(const char* field);

static bool parseBoundedIntArg(const char* field, int minValue, int maxValue, int& valueOut) {
  if (!server.hasArg(field)) return true;

  String raw = server.arg(field);
  raw.trim();
  if (raw.isEmpty()) {
    sendRangeError(field, minValue, maxValue);
    return false;
  }

  char* endPtr = nullptr;
  long parsed = strtol(raw.c_str(), &endPtr, 10);
  if (endPtr == raw.c_str() || *endPtr != '\0' || parsed < minValue || parsed > maxValue) {
    sendRangeError(field, minValue, maxValue);
    return false;
  }

  valueOut = static_cast<int>(parsed);
  return true;
}

static bool isDigitChar(char value) {
  return value >= '0' && value <= '9';
}

static bool parseClockTimeArg(const char* field, int& valueOut) {
  if (!server.hasArg(field)) return true;

  String raw = server.arg(field);
  raw.trim();
  if (raw.length() == 5 && raw.charAt(2) == ':' &&
      isDigitChar(raw.charAt(0)) && isDigitChar(raw.charAt(1)) &&
      isDigitChar(raw.charAt(3)) && isDigitChar(raw.charAt(4))) {
    int hour = raw.substring(0, 2).toInt();
    int minute = raw.substring(3, 5).toInt();
    if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
      valueOut = hour * 60 + minute;
      return true;
    }
  }

  char* endPtr = nullptr;
  long parsed = strtol(raw.c_str(), &endPtr, 10);
  if (endPtr != raw.c_str() && *endPtr == '\0' && parsed >= 0 && parsed <= 1439) {
    valueOut = static_cast<int>(parsed);
    return true;
  }

  sendAllowedValueError(field);
  return false;
}

static void sendAllowedValueError(const char* field) {
  DynamicJsonDocument doc(256);
  doc["success"] = false;
  doc["error"] = "invalid_value";
  doc["field"] = field;
  sendJsonDocument(400, doc);
}

static String redactWebParamForLog(const String& name, const String& value) {
  String lowerName = name;
  lowerName.toLowerCase();
  if (lowerName.indexOf("password") >= 0 || lowerName.indexOf("token") >= 0 ||
      lowerName.indexOf("key") >= 0 || lowerName.indexOf("webhook") >= 0 ||
      lowerName.indexOf("secret") >= 0 || lowerName.indexOf("chatid") >= 0) {
    return value.isEmpty() ? "" : "[redacted]";
  }
  if (lowerName.indexOf("message") >= 0 || lowerName.indexOf("content") >= 0) {
    String summary = "len=";
    summary += String(value.length());
    return summary;
  }
  return value;
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
  server.on("/api/debug/at/status", HTTP_GET, AUTH_WRAP(handleDebugATStatus));
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
  server.on("/api/config/led", HTTP_POST, AUTH_WRAP(handleSetLEDConfig));
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
  server.on("/api/sms/send/status", HTTP_GET, AUTH_WRAP(handleSendSMSStatus));
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

  DynamicJsonDocument doc(2560);
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
  doc["ledEnabled"] = config.led.enabled;
  doc["ledBrightness"] = config.led.brightness;
  doc["ledQuietHoursEnabled"] = config.led.quietHoursEnabled;
  doc["ledQuietActive"] = isLedQuietHoursActive();
  doc["battery"] = battery.percentage;
  doc["batteryDisplay"] = battery.displayPercentage;
  doc["batteryAvailable"] = battery.available;
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
  IPAddress dns2 = WiFi.dnsIP(1);
  wifi["dns2Current"] = (dns2 == IPAddress(0, 0, 0, 0)) ? "" : dns2.toString();
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
    
    if (!saveConfigOrSendError()) return;
    
    LOGI("WEB", "web_wifi_updated", ssid.c_str());
    server.send(200, "application/json", "{\"success\":true}");

    connectWiFi();
  } else {
    server.send(400, "application/json", jsonError("web_err_ssid_empty"));
  }
}

void handleGetBattery() {
  touchActivity();
  BatteryInfo battery = getBatteryInfo();
  DynamicJsonDocument doc(512);
  doc["available"] = battery.available;
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
  if (hasActiveModemAsyncJob()) {
    server.send(409, "application/json", "{\"success\":false,\"error\":\"modem_job_busy\"}");
    return;
  }
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

  uint32_t jobId = 0;
  ModemAsyncSubmitStatus submitStatus = queueAsyncATCommand(command, jobId);
  if (submitStatus == MODEM_ASYNC_SUBMIT_BUSY) {
    server.send(409, "application/json", "{\"success\":false,\"error\":\"modem_job_busy\"}");
    return;
  }
  if (submitStatus != MODEM_ASYNC_SUBMIT_ACCEPTED) {
    server.send(503, "application/json", "{\"success\":false,\"error\":\"job_lock_failed\"}");
    return;
  }

  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["queued"] = true;
  doc["jobId"] = jobId;
  sendJsonDocument(202, doc);
}

void handleDebugATStatus() {
  touchActivity();
  uint32_t jobId = server.hasArg("id") ? static_cast<uint32_t>(server.arg("id").toInt()) : 0;
  ModemAsyncJobResult job;
  if (!getAsyncATJobResult(jobId, job)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"job_not_found\"}");
    return;
  }

  DynamicJsonDocument doc(1536);
  doc["success"] = true;
  doc["queued"] = !job.running && !job.complete;
  doc["running"] = job.running;
  doc["complete"] = job.complete;
  doc["jobId"] = job.id;
  if (job.complete) {
    doc["response"] = job.response;
  }
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
  if (!saveConfigOrSendError()) return;
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
    String redactedValue = redactWebParamForLog(server.argName(i), server.arg(i));
    LOGD("WEB", "web_param", server.argName(i).c_str(), redactedValue.c_str());
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
  
  LOGI("WEB", "web_bark_config", config.bark.enabled ? i18nGet("bool_true") : i18nGet("bool_false"), "[redacted]");
  
  // Save config / 保存配置
  if (!saveConfigOrSendError()) return;
  
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
    LOGI("TEST", "web_bark_test", "[redacted]", config.bark.url.c_str());
    bool success = notificationManager.sendToBark(testTitle, testMessage);
    results["bark"] = success;
    if (success) successCount++;
  } else {
    LOGI("TEST", "web_bark_not_enabled", config.bark.enabled ? i18nGet("bool_true") : i18nGet("bool_false"), "[redacted]");
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
  int lowThreshold = config.battery.lowThreshold;
  int criticalThreshold = config.battery.criticalThreshold;
  if (!parseBoundedIntArg("lowThreshold", 5, 50, lowThreshold)) return;
  if (!parseBoundedIntArg("criticalThreshold", 1, 20, criticalThreshold)) return;
  if (criticalThreshold >= lowThreshold) {
    criticalThreshold = lowThreshold - 1;
    if (criticalThreshold < 1) criticalThreshold = 1;
  }

  config.battery.lowThreshold = lowThreshold;
  config.battery.criticalThreshold = criticalThreshold;
  config.battery.alertEnabled = server.hasArg("battery-alert-enabled");
  config.battery.lowBatteryAlertEnabled = server.hasArg("low-battery-alert-enabled");
  config.battery.chargingAlertEnabled = server.hasArg("charging-alert-enabled");
  config.battery.fullChargeAlertEnabled = server.hasArg("full-charge-alert-enabled");
  
  if (!saveConfigOrSendError()) return;
  LOGI("WEB", "web_battery_updated");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetLEDConfig() {
  touchActivity();
  int brightness = config.led.brightness;
  int quietStartMinutes = config.led.quietStartMinutes;
  int quietEndMinutes = config.led.quietEndMinutes;

  if (!parseBoundedIntArg("brightness", 1, 100, brightness)) return;
  if (!parseClockTimeArg("quietStart", quietStartMinutes)) return;
  if (!parseClockTimeArg("quietEnd", quietEndMinutes)) return;

  bool quietHoursEnabled = server.hasArg("led-quiet-enabled");
  if (quietHoursEnabled && quietStartMinutes == quietEndMinutes) {
    sendAllowedValueError("quietEnd");
    return;
  }

  config.led.enabled = server.hasArg("led-enabled");
  config.led.brightness = brightness;
  config.led.quietHoursEnabled = quietHoursEnabled;
  config.led.quietStartMinutes = quietStartMinutes;
  config.led.quietEndMinutes = quietEndMinutes;

  if (!saveConfigOrSendError()) return;
  if (isLedOutputSuppressed()) {
    setRGBLED(0, 0, 0);
  } else {
    updateSystemLED();
  }
  LOGI("WEB", "web_led_config_updated");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetNetworkConfig() {
  touchActivity();
  int signalCheckInterval = config.network.signalCheckInterval;
  int operatorMode = config.network.operatorMode;
  int radioMode = config.network.radioMode;
  int dataPolicy = config.network.dataPolicy;
  if (!parseBoundedIntArg("signalCheckInterval", 10, 300, signalCheckInterval)) return;
  if (!parseBoundedIntArg("operatorMode", 0, 4, operatorMode)) return;
  if (!parseBoundedIntArg("radioMode", 2, 38, radioMode)) return;
  if (server.hasArg("radioMode") && radioMode != 2 && radioMode != 38) {
    sendAllowedValueError("radioMode");
    return;
  }
  if (!parseBoundedIntArg("dataPolicy", DATA_POLICY_ALWAYS_OFF, DATA_POLICY_ALWAYS_ON, dataPolicy)) return;

  config.network.roamingAlertEnabled = server.hasArg("roaming-alert-enabled");
  config.network.autoDisableDataRoaming = server.hasArg("auto-disable-data-roaming");
  config.network.allowSmsDataRoaming = server.hasArg("allow-sms-data-roaming");
  config.network.signalCheckInterval = signalCheckInterval;
  config.network.operatorMode = operatorMode;
  config.network.radioMode = radioMode;
  config.network.dataPolicy = dataPolicy;
  if (server.hasArg("apn")) config.network.apn = server.arg("apn");
  if (server.hasArg("apnUser")) config.network.apnUser = server.arg("apnUser");
  if (server.hasArg("apnPass")) config.network.apnPass = server.arg("apnPass");
  
  if (!saveConfigOrSendError()) return;
  LOGI("WEB", "web_network_updated");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetSMSFilterConfig() {
  touchActivity();
  config.smsFilter.whitelistEnabled = server.hasArg("whitelist-enabled");
  config.smsFilter.keywordFilterEnabled = server.hasArg("keyword-filter-enabled");
  if (server.hasArg("whitelist")) config.smsFilter.whitelist = server.arg("whitelist");
  if (server.hasArg("blockedKeywords")) config.smsFilter.blockedKeywords = server.arg("blockedKeywords");

  if (!saveConfigOrSendError()) return;
  smsFilter.loadFromConfigStrings(config.smsFilter.whitelist, config.smsFilter.blockedKeywords);
  LOGI("WEB", "web_smsfilter_updated");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetSystemConfig() {
  touchActivity();
  int reportHour = config.reporting.reportHour;
  int sleepTimeout = config.sleep.timeout;
  int sleepMode = config.sleep.mode;
  int watchdogTimeout = config.watchdog.timeout;
  int timezoneOffsetMinutes = config.time.timezoneOffsetMinutes;
  if (!parseBoundedIntArg("reportHour", 0, 23, reportHour)) return;
  if (!parseBoundedIntArg("sleep-timeout", 60, 86400, sleepTimeout)) return;
  if (!parseBoundedIntArg("sleep-mode", 0, 1, sleepMode)) return;
  if (!parseBoundedIntArg("wdt-timeout", 10, 300, watchdogTimeout)) return;
  if (!parseBoundedIntArg("timezoneOffsetMinutes", -720, 840, timezoneOffsetMinutes)) return;
  
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

  config.reporting.dailyReportEnabled = server.hasArg("daily-report-enabled");
  config.reporting.weeklyReportEnabled = server.hasArg("weekly-report-enabled");
  config.reporting.reportHour = reportHour;
  config.time.timezoneOffsetMinutes = timezoneOffsetMinutes;
  config.debug.atCommandEcho = server.hasArg("at-command-echo");
  config.sleep.enabled = server.hasArg("sleep-enabled");
  config.sleep.timeout = sleepTimeout;
  config.sleep.mode = sleepMode;
  config.watchdog.timeout = watchdogTimeout;

  if (!saveConfigOrSendError()) return;
  sleepManager.configure(config.sleep.enabled, config.sleep.timeout, config.sleep.mode);
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
  doc["timezoneOffsetMinutes"] = getConfiguredTimezoneOffsetMinutes();
  sendJsonDocument(200, doc);
}

void handleGetSMS() {
  touchActivity();
  Statistics stats = statisticsManager.getStatistics();
  size_t total = static_cast<size_t>(smsStorage.getSMSCount());
  size_t offset = readSizeArgOrDefault("offset", 0, total);
  size_t limit = readSizeArgOrDefault("limit", total, MAX_SMS_COUNT);
  if (offset > total) offset = total;
  if (limit > MAX_SMS_COUNT) limit = MAX_SMS_COUNT;
  size_t available = total - offset;
  size_t returned = (limit < available) ? limit : available;

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  String header = "{\"stats\":{\"received\":";
  header += String(stats.totalSMSReceived);
  header += ",\"forwarded\":";
  header += String(stats.totalSMSForwarded);
  header += ",\"filtered\":";
  header += String(stats.totalSMSFiltered);
  header += ",\"stored\":";
  header += String(total);
  header += "},\"pagination\":{\"offset\":";
  header += String(offset);
  header += ",\"limit\":";
  header += String(limit);
  header += ",\"returned\":";
  header += String(returned);
  header += ",\"total\":";
  header += String(total);
  header += "},\"messages\":[";
  server.sendContent(header);

  bool first = true;
  for (size_t index = offset; index < offset + returned; index++) {
    SMSRecord record;
    if (!smsStorage.getSMSAt(index, record)) continue;

    String item;
    item.reserve(record.sender.length() + record.content.length() + record.timestamp.length() +
                 record.status.length() + record.lastAttemptAt.length() + record.lastError.length() + 220);
    if (!first) item += ',';
    item += "{\"id\":";
    item += String(record.id);
    appendJsonStringMember(item, "sender", record.sender);
    appendJsonStringMember(item, "content", record.content);
    appendJsonStringMember(item, "timestamp", record.timestamp);
    appendJsonStringMember(item, "status", record.status);
    item += ",\"forwarded\":";
    item += SMSStorage::isSuccessStatus(record.status) ? "true" : "false";
    item += ",\"retryCount\":";
    item += String(record.retryCount);
    appendJsonStringMember(item, "lastAttemptAt", record.lastAttemptAt);
    appendJsonStringMember(item, "lastError", record.lastError);
    item += ",\"canManualForward\":";
    item += SMSStorage::canManualForward(record.status) ? "true" : "false";
    item += '}';
    server.sendContent(item);
    watchdogManager.feedWatchdog();
    first = false;
  }

  server.sendContent("]}");
  server.sendContent("");
}

void handleClearSMS() {
  touchActivity();
  if (!smsStorage.clearAllSMS()) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"sms_storage_write_failed\"}");
    return;
  }
  notificationManager.cancelAllSMS();
  retryManager.clearRetries();
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

  if (smsStorage.getSMSById(smsId).id == 0) {
    server.send(404, "application/json", jsonError("web_err_sms_not_found"));
    return;
  }
  if (!smsStorage.deleteSMS(smsId)) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"sms_storage_write_failed\"}");
    return;
  }

  notificationManager.cancelSMS(smsId);
  retryManager.cancelRetry(smsId);
  LOGI("WEB", "web_sms_deleted", id.c_str());
  server.send(200, "application/json", "{\"success\":true}");
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
  bool queued = notificationManager.forwardSMS(sms.sender, sms.content, false, smsId, true);
  if (queued) {
    LOGI("WEB", "web_sms_forward_success", id.c_str());
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(503, "application/json", jsonError("web_sms_forward_fail"));
  }
}

void handleResetSIM() {
  touchActivity();
  if (hasActiveModemAsyncJob()) {
    server.send(409, "application/json", "{\"success\":false,\"error\":\"modem_job_busy\"}");
    return;
  }
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
  doc["batteryAvailable"] = battery.available;
  doc["voltage"] = battery.voltage;
  doc["isCharging"] = battery.isCharging;
  doc["memory"] = ESP.getFreeHeap() / 1024;
  doc["uptime"] = millis() / 1000;
  doc["lastUpdate"] = sysStatus.lastUpdate;
  sendJsonDocument(200, doc);
}

void handleRefreshSystemStatus() {
  touchActivity();
  if (hasActiveModemAsyncJob()) {
    server.send(409, "application/json", "{\"success\":false,\"error\":\"modem_job_busy\"}");
    return;
  }
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

  uint32_t jobId = 0;
  ModemAsyncSubmitStatus submitStatus = queueAsyncSMS(phoneNumber, message, jobId);
  if (submitStatus == MODEM_ASYNC_SUBMIT_BUSY) {
    server.send(409, "application/json", "{\"success\":false,\"error\":\"modem_job_busy\"}");
    return;
  }
  if (submitStatus != MODEM_ASYNC_SUBMIT_ACCEPTED) {
    server.send(503, "application/json", "{\"success\":false,\"error\":\"job_lock_failed\"}");
    return;
  }

  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["queued"] = true;
  doc["jobId"] = jobId;
  doc["message"] = i18nGet("web_sms_send_queued");
  sendJsonDocument(202, doc);
}

void handleSendSMSStatus() {
  touchActivity();
  uint32_t jobId = server.hasArg("id") ? static_cast<uint32_t>(server.arg("id").toInt()) : 0;
  ModemAsyncJobResult job;
  if (!getAsyncSMSJobResult(jobId, job)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"job_not_found\"}");
    return;
  }

  DynamicJsonDocument doc(512);
  doc["success"] = true;
  doc["queued"] = !job.running && !job.complete;
  doc["running"] = job.running;
  doc["complete"] = job.complete;
  doc["jobId"] = job.id;
  if (job.complete) {
    doc["sent"] = job.success;
    if (!job.success) {
      doc["error"] = job.error;
    }
  }
  sendJsonDocument(200, doc);
}

void handleDebugEcho() {
  touchActivity();
  bool enabled = false;
  if (!readJsonBoolField("enabled", enabled)) {
    server.send(400, "application/json", jsonError("web_missing_enabled"));
    return;
  }
  config.debug.atCommandEcho = enabled;
  if (!saveConfigOrSendError()) return;

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
  if (hasActiveModemAsyncJob()) {
    server.send(409, "application/json", "{\"success\":false,\"error\":\"modem_job_busy\"}");
    return;
  }
  if (simState != SIM_STATE_READY) {
    server.send(400, "application/json", jsonError("web_sim_not_ready"));
    return;
  }
  
  LOGI("WEB", "web_sms_check");
  checkAllSMS();
  
  server.send(200, "application/json", jsonMessage("web_sms_check_started"));
}
