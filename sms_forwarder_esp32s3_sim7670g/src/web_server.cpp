#include "web_server.h"
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

WebServer server(80);

inline void touchActivity() {
  sleepManager.updateActivity();
}

static String escapeJson(const String& input) {
  String out = "";
  out.reserve(input.length() + 8);
  for (int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((unsigned char)c < 0x20) {
          // skip other control chars
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
}

void initWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });
  
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/battery", HTTP_GET, handleGetBattery);
  server.on("/api/system/version", HTTP_GET, handleGetVersion);
  server.on("/api/system/info", HTTP_GET, handleGetSystemInfo);
  server.on("/api/logs", HTTP_GET, handleGetLogs);
  server.on("/api/logs", HTTP_DELETE, handleClearLogs);
  server.on("/api/statistics", HTTP_GET, handleGetStatistics);
  server.on("/api/statistics", HTTP_DELETE, handleResetStatistics);
  server.on("/api/debug/restart", HTTP_POST, handleDebugRestart);
  server.on("/api/debug/system", HTTP_GET, handleDebugSystem);
  server.on("/api/debug/at", HTTP_POST, handleDebugAT);
  server.on("/api/debug/wifi", HTTP_POST, handleDebugWiFi);
  server.on("/api/debug/network", HTTP_POST, handleDebugNetwork);
  server.on("/api/debug/notification", HTTP_POST, handleDebugNotification);
  server.on("/api/debug/echo", HTTP_POST, handleDebugEcho);
  server.on("/api/debug/led", HTTP_POST, handleDebugLED);
  server.on("/api/config/wifi", HTTP_POST, handleSetConfig);
  server.on("/api/config/notification", HTTP_POST, handleSetNotificationConfig);
  server.on("/api/config/battery", HTTP_POST, handleSetBatteryConfig);
  server.on("/api/config/network", HTTP_POST, handleSetNetworkConfig);
  server.on("/api/config/smsfilter", HTTP_POST, handleSetSMSFilterConfig);
  server.on("/api/config/system", HTTP_POST, handleSetSystemConfig);
  server.on("/api/test/notification", HTTP_POST, handleTestNotification);
  server.on("/api/sim/reset", HTTP_POST, handleResetSIM);

  server.on("/api/sms", HTTP_GET, handleGetSMS);
  server.on("/api/sms", HTTP_DELETE, handleClearSMS);
  server.on("/api/sms/delete", HTTP_POST, handleDeleteSMS);
  server.on("/api/sms/forward", HTTP_POST, handleForwardSMS);
  server.on("/api/sms/send", HTTP_POST, handleSendSMS);
  server.on("/api/sms/check", HTTP_POST, handleCheckSMS);
  server.on("/api/forward-status", HTTP_GET, handleGetForwardStatus);
  server.on("/api/system/status", HTTP_GET, handleGetSystemStatus);
  server.on("/api/system/refresh", HTTP_POST, handleRefreshSystemStatus);
  
  server.begin();
  Serial.println("Web server started");
}

void handleGetStatus() {
  touchActivity();
  // 使用系统状态缓存
  SystemStatus sysStatus = systemStatus.getStatus();
  BatteryInfo battery = getBatteryInfo();
  
  String response = "{\"signal\":" + String(sysStatus.signalStrength);
  response += ",\"network\":\"" + String(sysStatus.networkConnected ? "Connected" : "Disconnected") + "\"";
  response += ",\"simStatus\":\"" + String(sysStatus.simReady ? "Ready" : "Not Ready") + "\"";
  response += ",\"operator\":\"" + sysStatus.operatorName + "\"";
  response += ",\"operatorCode\":\"" + sysStatus.operatorCode + "\"";
  response += ",\"homeOperator\":\"" + sysStatus.homeOperatorName + "\"";
  response += ",\"homeOperatorCode\":\"" + sysStatus.homeOperatorCode + "\"";
  response += ",\"networkType\":\"" + sysStatus.networkType + "\"";
  response += ",\"isRoaming\":" + String(sysStatus.isRoaming ? "true" : "false");
  response += ",\"smsAvailable\":" + String(sysStatus.networkConnected ? "true" : "false");
  response += ",\"csRegistered\":" + String(sysStatus.csRegistered ? "true" : "false");
  response += ",\"epsRegistered\":" + String(sysStatus.epsRegistered ? "true" : "false");
  response += ",\"dataAttached\":" + String(sysStatus.dataAttached ? "true" : "false");
  response += ",\"dataPolicy\":" + String(config.network.dataPolicy);
  response += ",\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  response += ",\"wifiRssi\":" + String(WiFi.RSSI());
  response += ",\"wifiIp\":\"" + WiFi.localIP().toString() + "\"";
  response += ",\"ledStatus\":\"" + String(getLedStatus()) + "\"";
  response += ",\"ledReason\":\"" + String(getLedReason()) + "\"";
  response += ",\"battery\":" + String(battery.percentage, 1);
  response += ",\"voltage\":" + String(battery.voltage, 2);
  response += ",\"isCharging\":" + String(battery.isCharging ? "true" : "false");
  response += ",\"memory\":" + String(ESP.getFreeHeap() / 1024);
  response += ",\"timestamp\":" + String(millis());
  response += "}";
  
  server.send(200, "application/json", response);
}

void handleGetConfig() {
  touchActivity();
  String response = "{";
  
  // WiFi配置
  response += "\"wifi\":{";
  response += "\"ssid\":\"" + config.wifi.ssid + "\",";
  response += "\"password\":\"" + config.wifi.password + "\",";
  response += "\"useCustomDns\":" + String(config.wifi.useCustomDns ? "true" : "false") + ",";
  response += "\"forceStaticDns\":" + String(config.wifi.forceStaticDns ? "true" : "false") + ",";
  response += "\"staticIp\":\"" + config.wifi.staticIp + "\",";
  response += "\"staticGateway\":\"" + config.wifi.staticGateway + "\",";
  response += "\"staticSubnet\":\"" + config.wifi.staticSubnet + "\",";
  response += "\"dns1\":\"" + config.wifi.dns1 + "\",";
  response += "\"dns2\":\"" + config.wifi.dns2 + "\",";
  response += "\"dns1Current\":\"" + WiFi.dnsIP(0).toString() + "\",";
  response += "\"dns2Current\":\"" + WiFi.dnsIP(1).toString() + "\"";
  response += "},";
  
  // Bark配置
  response += "\"bark\":{";
  response += "\"enabled\":" + String(config.bark.enabled ? "true" : "false") + ",";
  response += "\"key\":\"" + config.bark.key + "\",";
  response += "\"url\":\"" + config.bark.url + "\"";
  response += "},";
  
  // Server酱配置
  response += "\"serverChan\":{";
  response += "\"enabled\":" + String(config.serverChan.enabled ? "true" : "false") + ",";
  response += "\"key\":\"" + config.serverChan.key + "\",";
  response += "\"url\":\"" + config.serverChan.url + "\"";
  response += "},";
  
  // Telegram配置
  response += "\"telegram\":{";
  response += "\"enabled\":" + String(config.telegram.enabled ? "true" : "false") + ",";
  response += "\"token\":\"" + config.telegram.token + "\",";
  response += "\"chatId\":\"" + config.telegram.chatId + "\",";
  response += "\"url\":\"" + config.telegram.url + "\"";
  response += "},";
  
  // 钉钉配置
  response += "\"dingtalk\":{";
  response += "\"enabled\":" + String(config.dingtalk.enabled ? "true" : "false") + ",";
  response += "\"webhook\":\"" + config.dingtalk.webhook + "\"";
  response += "},";
  
  // 飞书配置
  response += "\"feishu\":{";
  response += "\"enabled\":" + String(config.feishu.enabled ? "true" : "false") + ",";
  response += "\"webhook\":\"" + config.feishu.webhook + "\"";
  response += "},";
  
  // 自定义配置
  response += "\"custom\":{";
  response += "\"enabled\":" + String(config.custom.enabled ? "true" : "false") + ",";
  response += "\"url\":\"" + config.custom.url + "\",";
  response += "\"key\":\"" + config.custom.key + "\"";
  response += "},";
  
  // 电池配置
  response += "\"battery\":{";
  response += "\"lowThreshold\":" + String(config.battery.lowThreshold) + ",";
  response += "\"criticalThreshold\":" + String(config.battery.criticalThreshold) + ",";
  response += "\"alertEnabled\":" + String(config.battery.alertEnabled ? "true" : "false") + ",";
  response += "\"lowBatteryAlertEnabled\":" + String(config.battery.lowBatteryAlertEnabled ? "true" : "false");
  response += "},";

  // 休眠配置
  response += "\"sleep\":{";
  response += "\"enabled\":" + String(config.sleep.enabled ? "true" : "false") + ",";
  response += "\"timeout\":" + String(config.sleep.timeout) + ",";
  response += "\"mode\":" + String(config.sleep.mode);
  response += "},";
  
  // 网络配置
  response += "\"network\":{";
  response += "\"roamingAlertEnabled\":" + String(config.network.roamingAlertEnabled ? "true" : "false") + ",";
  response += "\"autoDisableDataRoaming\":" + String(config.network.autoDisableDataRoaming ? "true" : "false") + ",";
  response += "\"signalCheckInterval\":" + String(config.network.signalCheckInterval) + ",";
  response += "\"operatorMode\":" + String(config.network.operatorMode) + ",";
  response += "\"radioMode\":" + String(config.network.radioMode) + ",";
  response += "\"dataPolicy\":" + String(config.network.dataPolicy) + ",";
  response += "\"apn\":\"" + config.network.apn + "\",";
  response += "\"apnUser\":\"" + config.network.apnUser + "\",";
  response += "\"apnPass\":\"" + config.network.apnPass + "\"";
  response += "},";
  
  // 短信过滤配置
  response += "\"smsFilter\":{";
  response += "\"whitelistEnabled\":" + String(config.smsFilter.whitelistEnabled ? "true" : "false") + ",";
  response += "\"keywordFilterEnabled\":" + String(config.smsFilter.keywordFilterEnabled ? "true" : "false") + ",";
  response += "\"whitelist\":\"" + config.smsFilter.whitelist + "\",";
  response += "\"blockedKeywords\":\"" + config.smsFilter.blockedKeywords + "\"";
  response += "},";
  
  // 报告配置
  response += "\"reporting\":{";
  response += "\"dailyReportEnabled\":" + String(config.reporting.dailyReportEnabled ? "true" : "false") + ",";
  response += "\"weeklyReportEnabled\":" + String(config.reporting.weeklyReportEnabled ? "true" : "false") + ",";
  response += "\"reportHour\":" + String(config.reporting.reportHour);
  response += "},";
  
  // 看门狗配置
  response += "\"watchdog\":{";
  response += "\"timeout\":" + String(config.watchdog.timeout);
  response += "},";
  
  // 调试配置
  response += "\"debug\":{";
  response += "\"atCommandEcho\":" + String(config.debug.atCommandEcho ? "true" : "false");
  response += "}";
  
  response += "}";
  server.send(200, "application/json", response);
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
    
    logManager.addLog(LOG_INFO, "WEB", "WiFi配置已更新: " + ssid);
    server.send(200, "application/json", "{\"success\":true}");
    
    // 延迟重启连接WiFi
    delay(1000);
    WiFi.disconnect();
    delay(500);
    connectWiFi();
  } else {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"SSID不能为空\"}");
  }
}

void handleGetBattery() {
  touchActivity();
  BatteryInfo battery = getBatteryInfo();
  String response = "{\"voltage\":" + String(battery.voltage, 2);
  response += ",\"percentage\":" + String(battery.percentage, 1);
  response += ",\"isCharging\":" + String(battery.isCharging ? "true" : "false");
  response += ",\"isLowBattery\":" + String(battery.isLowBattery ? "true" : "false");
  response += ",\"chargeRate\":" + String(battery.chargeRate, 2);
  response += ",\"timestamp\":" + String(millis());
  response += "}";
  server.send(200, "application/json", response);
}

void handleDebugSystem() {
  touchActivity();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
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
    server.send(400, "application/json", "{\"error\":\"Missing command\"}");
    return;
  }
  
  String response = escapeJson(sendATCommand(command));
  server.send(200, "application/json", "{\"response\":\"" + response + "\"}");
}

void handleDebugWiFi() {
  touchActivity();
  diagnoseWiFi();
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
  String response = "{\"version\":\"1.0.0\",\"buildTime\":\"";
  response += __DATE__;
  response += " ";
  response += __TIME__;
  response += "\",\"chipModel\":\"";
  response += ESP.getChipModel();
  response += "\"}";
  server.send(200, "application/json", response);
}

void handleGetStatistics() {
  touchActivity();
  Statistics stats = statisticsManager.getStatistics();
  String response = "{\"totalSMSReceived\":" + String(stats.totalSMSReceived);
  response += ",\"totalSMSForwarded\":" + String(stats.totalSMSForwarded);
  response += ",\"totalSMSFiltered\":" + String(stats.totalSMSFiltered);
  response += ",\"totalPushSuccess\":" + String(stats.totalPushSuccess);
  response += ",\"totalPushFailed\":" + String(stats.totalPushFailed);
  response += ",\"uptime\":" + String(stats.uptime);
  response += "}";
  server.send(200, "application/json", response);
}

void handleResetStatistics() {
  touchActivity();
  statisticsManager.resetStatistics();
  logManager.addLog(LOG_INFO, "WEB", "统计数据已重置");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleGetLogs() {
  touchActivity();
  String minLevel = server.arg("level");
  String filter = server.arg("filter");
  uint8_t level = minLevel.isEmpty() ? 0 : minLevel.toInt();
  
  String response = logManager.getLogsAsJson(level, filter);
  server.send(200, "application/json", response);
}

void handleClearLogs() {
  touchActivity();
  logManager.clearLogs();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetNotificationConfig() {
  touchActivity();
  logManager.addLog(LOG_INFO, "WEB", "开始更新推送配置");
  
  // 调试：打印所有参数
  for (int i = 0; i < server.args(); i++) {
    logManager.addLog(LOG_DEBUG, "WEB", "Param: " + server.argName(i) + "=" + server.arg(i));
  }
  
  // Bark配置
  if (server.hasArg("barkKey")) config.bark.key = server.arg("barkKey");
  if (server.hasArg("barkUrl")) config.bark.url = server.arg("barkUrl");
  
  // Server酱配置
  if (server.hasArg("serverChanKey")) config.serverChan.key = server.arg("serverChanKey");
  if (server.hasArg("serverChanUrl")) config.serverChan.url = server.arg("serverChanUrl");
  
  // Telegram配置
  if (server.hasArg("telegramToken")) config.telegram.token = server.arg("telegramToken");
  if (server.hasArg("telegramChatId")) config.telegram.chatId = server.arg("telegramChatId");
  if (server.hasArg("telegramUrl")) config.telegram.url = server.arg("telegramUrl");
  
  // 钉钉配置
  if (server.hasArg("dingtalkWebhook")) config.dingtalk.webhook = server.arg("dingtalkWebhook");
  
  // 飞书配置
  if (server.hasArg("feishuWebhook")) config.feishu.webhook = server.arg("feishuWebhook");
  
  // 自定义配置
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
  
  logManager.addLog(LOG_INFO, "WEB", "Bark配置: enabled=" + String(config.bark.enabled ? "true" : "false") + ", key=" + config.bark.key);
  
  // 保存配置
  saveConfig();
  
  logManager.addLog(LOG_INFO, "WEB", "推送配置已更新");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleTestNotification() {
  touchActivity();
  logManager.addLog(LOG_INFO, "TEST", "开始测试推送");
  
  String testMessage = "短信转发器测试消息 - " + String(millis());
  String testTitle = "测试推送";
  
  // 测试所有已启用的推送渠道
  String response = "{\"results\":{";
  bool first = true;
  int totalTests = 0;
  int successCount = 0;
  
  if (config.bark.enabled && !config.bark.key.isEmpty()) {
    if (!first) response += ",";
    totalTests++;
    logManager.addLog(LOG_INFO, "TEST", "Bark测试: key=" + config.bark.key + ", url=" + config.bark.url);
    bool success = notificationManager.sendToBark(testTitle, testMessage);
    response += "\"bark\":" + String(success ? "true" : "false");
    if (success) successCount++;
    first = false;
  } else {
    logManager.addLog(LOG_INFO, "TEST", "Bark未启用: enabled=" + String(config.bark.enabled) + ", key=" + config.bark.key);
  }
  
  if (config.serverChan.enabled && !config.serverChan.key.isEmpty()) {
    if (!first) response += ",";
    totalTests++;
    bool success = notificationManager.sendToServerChan(testTitle, testMessage);
    response += "\"serverChan\":" + String(success ? "true" : "false");
    if (success) successCount++;
    first = false;
  }
  
  if (config.telegram.enabled && !config.telegram.token.isEmpty()) {
    if (!first) response += ",";
    totalTests++;
    bool success = notificationManager.sendToTelegram(testTitle, testMessage);
    response += "\"telegram\":" + String(success ? "true" : "false");
    if (success) successCount++;
    first = false;
  }
  
  response += "},\"total\":" + String(totalTests) + ",\"success\":" + String(successCount) + "}";
  server.send(200, "application/json", response);
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
  logManager.addLog(LOG_INFO, "WEB", "电池配置已更新");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetNetworkConfig() {
  touchActivity();
  config.network.roamingAlertEnabled = server.hasArg("roaming-alert-enabled");
  config.network.autoDisableDataRoaming = server.hasArg("auto-disable-data-roaming");
  if (server.hasArg("signalCheckInterval")) config.network.signalCheckInterval = server.arg("signalCheckInterval").toInt();
  if (server.hasArg("operatorMode")) config.network.operatorMode = server.arg("operatorMode").toInt();
  if (server.hasArg("radioMode")) config.network.radioMode = server.arg("radioMode").toInt();
  if (server.hasArg("dataPolicy")) config.network.dataPolicy = server.arg("dataPolicy").toInt();
  if (server.hasArg("apn")) config.network.apn = server.arg("apn");
  if (server.hasArg("apnUser")) config.network.apnUser = server.arg("apnUser");
  if (server.hasArg("apnPass")) config.network.apnPass = server.arg("apnPass");
  
  saveConfig();
  logManager.addLog(LOG_INFO, "WEB", "网络配置已更新");
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
  logManager.addLog(LOG_INFO, "WEB", "短信过滤配置已更新");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleSetSystemConfig() {
  touchActivity();
  config.reporting.dailyReportEnabled = server.hasArg("daily-report-enabled");
  config.reporting.weeklyReportEnabled = server.hasArg("weekly-report-enabled");
  if (server.hasArg("reportHour")) config.reporting.reportHour = server.arg("reportHour").toInt();
  
  // 调试配置
  config.debug.atCommandEcho = server.hasArg("at-command-echo");
  
  // 休眠配置
  config.sleep.enabled = server.hasArg("sleep-enabled");
  if (server.hasArg("sleep-timeout")) config.sleep.timeout = server.arg("sleep-timeout").toInt();
  if (server.hasArg("sleep-mode")) config.sleep.mode = server.arg("sleep-mode").toInt();
  sleepManager.configure(config.sleep.enabled, config.sleep.timeout, config.sleep.mode);

  // 看门狗配置
  if (server.hasArg("wdt-timeout")) {
    config.watchdog.timeout = server.arg("wdt-timeout").toInt();
  }
  
  saveConfig();
  logManager.addLog(LOG_INFO, "WEB", "系统配置已更新");
  watchdogManager.disableWatchdog();
  watchdogManager.initWatchdog();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleGetSystemInfo() {
  touchActivity();
  static String cachedInfo = "";
  
  if (cachedInfo.isEmpty()) {
    uint32_t totalHeap = ESP.getHeapSize();
    cachedInfo = "{";
    cachedInfo += "\"totalMemory\":" + String(totalHeap / 1024);
    cachedInfo += ",\"cpuFreq\":" + String(ESP.getCpuFreqMHz());
    cachedInfo += ",\"flashSize\":" + String(ESP.getFlashChipSize() / (1024 * 1024));
    cachedInfo += ",\"chipModel\":\"" + String(ESP.getChipModel()) + "\"";
    cachedInfo += ",\"chipRevision\":" + String(ESP.getChipRevision());
    cachedInfo += ",\"chipCores\":" + String(ESP.getChipCores());
    cachedInfo += "}";
  }
  
  server.send(200, "application/json", cachedInfo);
}

void handleGetSMS() {
  touchActivity();
  Statistics stats = statisticsManager.getStatistics();
  String response = "{\"stats\":{";
  response += "\"received\":" + String(stats.totalSMSReceived);
  response += ",\"forwarded\":" + String(stats.totalSMSForwarded);
  response += ",\"filtered\":" + String(stats.totalSMSFiltered);
  response += ",\"stored\":" + String(smsStorage.getSMSCount());
  response += "},\"messages\":[";
  
  // 从存储中读取短信列表
  std::vector<SMSRecord> records = smsStorage.getAllSMS();
  for (size_t i = 0; i < records.size(); i++) {
    if (i > 0) response += ",";
    response += "{";
    String escapedSender = records[i].sender;
    String escapedContent = records[i].content;
    escapedSender.replace("\\", "\\\\");
    escapedSender.replace("\"", "\\\"");
    escapedContent.replace("\\", "\\\\");
    escapedContent.replace("\"", "\\\"");
    
    response += "\"id\":" + String(records[i].id);
    response += ",\"sender\":\"" + escapedSender + "\"";
    response += ",\"content\":\"" + escapedContent + "\"";
    response += ",\"timestamp\":\"" + records[i].timestamp + "\"";
    response += ",\"forwarded\":" + String(records[i].forwarded ? "true" : "false");
    response += "}";
  }
  
  response += "]}";
  server.send(200, "application/json", response);
}

void handleClearSMS() {
  touchActivity();
  smsStorage.clearAllSMS();
  logManager.addLog(LOG_INFO, "WEB", "短信存储已清空");
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
    server.send(400, "application/json", "{\"success\":false,\"error\":\"ID无效\"}");
    return;
  }
  
  bool removed = smsStorage.deleteSMS(smsId);
  if (removed) {
    logManager.addLog(LOG_INFO, "WEB", "删除短信: " + id);
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"短信不存在\"}");
  }
}

void handleForwardSMS() {
  touchActivity();
  String id = server.arg("id");
  if (id.isEmpty()) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing ID\"}");
    return;
  }
  
  // 获取短信内容
  SMSRecord sms = smsStorage.getSMSById(id.toInt());
  if (sms.id == 0) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"短信不存在\"}");
    return;
  }
  
  logManager.addLog(LOG_INFO, "WEB", "手动转发短信: " + id + ", 发送方: " + sms.sender);
  
  // 转发短信
  statisticsManager.incrementSMSForwarded();
  notificationManager.forwardSMS(sms.sender, sms.content);
  
  // 更新转发状态
  smsStorage.updateSMSForwardStatus(id.toInt(), true);
  
  logManager.addLog(LOG_INFO, "WEB", "短信转发成功: " + id);
  server.send(200, "application/json", "{\"success\":true}");
}

void handleResetSIM() {
  touchActivity();
  resetSIMCheck();
  server.send(200, "application/json", "{\"success\":true,\"message\":\"SIM检测已重置\"}");
}

void handleGetForwardStatus() {
  touchActivity();
  Statistics stats = statisticsManager.getStatistics();
  String response = "{\"platforms\":[";
  
  bool first = true;
  if (config.bark.enabled) {
    if (!first) response += ",";
    response += "{\"name\":\"Bark\",\"enabled\":true}";
    first = false;
  }
  
  if (config.serverChan.enabled) {
    if (!first) response += ",";
    response += "{\"name\":\"Server酱\",\"enabled\":true}";
    first = false;
  }
  
  if (config.telegram.enabled) {
    if (!first) response += ",";
    response += "{\"name\":\"Telegram\",\"enabled\":true}";
    first = false;
  }
  
  response += "],\"totalSuccess\":" + String(stats.totalPushSuccess);
  response += ",\"totalFailed\":" + String(stats.totalPushFailed) + "}";
  server.send(200, "application/json", response);
}

void handleGetSystemStatus() {
  touchActivity();
  SystemStatus sysStatus = systemStatus.getStatus();
  BatteryInfo battery = getBatteryInfo();
  
  String response = "{";
  response += "\"signal\":" + String(sysStatus.signalStrength) + ",";
  response += "\"simReady\":" + String(sysStatus.simReady ? "true" : "false") + ",";
  response += "\"networkConnected\":" + String(sysStatus.networkConnected ? "true" : "false") + ",";
  response += "\"csRegistered\":" + String(sysStatus.csRegistered ? "true" : "false") + ",";
  response += "\"epsRegistered\":" + String(sysStatus.epsRegistered ? "true" : "false") + ",";
  response += "\"dataAttached\":" + String(sysStatus.dataAttached ? "true" : "false") + ",";
  response += "\"dataPolicy\":" + String(config.network.dataPolicy) + ",";
  response += "\"operatorName\":\"" + sysStatus.operatorName + "\",";
  response += "\"networkType\":\"" + sysStatus.networkType + "\",";
  response += "\"isRoaming\":" + String(sysStatus.isRoaming ? "true" : "false") + ",";
  response += "\"battery\":" + String(battery.percentage, 1) + ",";
  response += "\"voltage\":" + String(battery.voltage, 2) + ",";
  response += "\"isCharging\":" + String(battery.isCharging ? "true" : "false") + ",";
  response += "\"memory\":" + String(ESP.getFreeHeap() / 1024) + ",";
  response += "\"uptime\":" + String(millis() / 1000) + ",";
  response += "\"lastUpdate\":" + String(sysStatus.lastUpdate);
  response += "}";
  
  server.send(200, "application/json", response);
}

void handleRefreshSystemStatus() {
  touchActivity();
  String type = server.arg("type");
  
  if (type == "signal") {
    systemStatus.refreshSignalOnly();
    server.send(200, "application/json", "{\"success\":true,\"message\":\"信号强度已刷新\"}");
  } else if (type == "all") {
    systemStatus.refreshAllStatus();
    server.send(200, "application/json", "{\"success\":true,\"message\":\"所有状态已刷新\"}");
  } else {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"无效的刷新类型\"}");
  }
}

void handleSendSMS() {
  touchActivity();
  String phoneNumber = server.arg("phoneNumber");
  String message = server.arg("message");
  
  if (phoneNumber.isEmpty() || message.isEmpty()) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"手机号和消息不能为空\"}");
    return;
  }
  
  if (simState != SIM_STATE_READY) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"SIM模块未就绪\"}");
    return;
  }
  
  bool success = sendSMS(phoneNumber, message);
  
  if (success) {
    server.send(200, "application/json", "{\"success\":true,\"message\":\"短信发送成功\"}");
  } else {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"短信发送失败\"}");
  }
}

void handleDebugEcho() {
  touchActivity();
  String body = server.arg("plain");
  
  // 解析JSON格式: {"enabled": true/false}
  int enabledPos = body.indexOf("\"enabled\":");
  if (enabledPos >= 0) {
    // 查找冒号后的值
    int colonPos = body.indexOf(":", enabledPos);
    if (colonPos > 0) {
      String valueStr = body.substring(colonPos + 1);
      valueStr.trim();
      valueStr.replace("}", "");
      valueStr.replace(" ", "");
      
      bool enabled = (valueStr == "true");
      
      config.debug.atCommandEcho = enabled;
      saveConfig();
      
      logManager.addLog(LOG_INFO, "WEB", "AT指令回显已" + String(enabled ? "开启" : "关闭"));
      server.send(200, "application/json", "{\"success\":true}");
    } else {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"无效JSON格式\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"缺少enabled参数\"}");
  }
}

void handleDebugLED() {
  touchActivity();
  String test = server.arg("test");
  
  if (test == "hardware") {
    testLEDHardware();
    server.send(200, "application/json", "{\"success\":true,\"message\":\"LED硬件测试完成\"}");
  } else if (test == "states") {
    testAllLEDStates();
    server.send(200, "application/json", "{\"success\":true,\"message\":\"LED状态测试完成\"}");
  } else {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"无效测试类型\"}");
  }
}

void handleCheckSMS() {
  touchActivity();
  if (simState != SIM_STATE_READY) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"SIM模块未就绪\"}");
    return;
  }
  
  logManager.addLog(LOG_INFO, "WEB", "手动查询短信");
  checkAllSMS();
  
  server.send(200, "application/json", "{\"success\":true,\"message\":\"短信查询已启动，请查看日志\"}");
}
