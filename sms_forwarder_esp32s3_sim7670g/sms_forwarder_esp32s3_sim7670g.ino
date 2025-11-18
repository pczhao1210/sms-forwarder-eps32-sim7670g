/*
 * SMS转发器 - ESP32-S3 + SIM7670G
 * 基于微雪ESP32-S3-SIM7670G-4G模组
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

#include "src/config_manager.h"
#include "src/sim7670g_manager.h"
#include "src/battery_manager.h"
#include "src/led_controller.h"
#include "src/wifi_manager.h"
#include "src/web_server.h"
#include "src/log_manager.h"

#include "src/notification_manager.h"

#include "src/retry_manager.h"
#include "src/sms_filter.h"
#include "src/memory_manager.h"
#include "src/sms_storage.h"
#include "src/network_manager.h"
#include "src/watchdog_manager.h"
#include "src/statistics_manager.h"

extern WebServer server;


// SMS处理函数声明
void sendDailyReport();
void sendWeeklyReport();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("SMS转发器启动中...");
  initLED();
  setStatusLED("init");
  
  // 初始化各模块
  Serial.println("[INIT] 开始初始化各模块...");
  
  Serial.print("[INIT] 配置管理器: ");
  initConfig();
  Serial.println("✓ 完成");
  sleepManager.configure(config.sleep.enabled, config.sleep.timeout, config.sleep.mode);
  sleepManager.updateActivity();
  
  // LED功能已移除
  
  Serial.print("[INIT] 电池管理器: ");
  bool batteryOk = initBatteryMonitor();
  Serial.println(batteryOk ? "✓ 完成" : "✗ 失败");
  
  Serial.print("[INIT] SMS存储: ");
  smsStorage.init();
  Serial.println("✓ 完成");
  
  Serial.print("[INIT] SIM7670G模组: ");
  initSIM7670G();
  Serial.println("✓ 完成");
  logManager.addLog(LOG_INFO, "SIM7670G", "SIM7670G初始化完成");
  
  Serial.print("[INIT] 网络管理器: ");
  networkManager.initNetwork();
  Serial.println("✓ 完成");
  
  Serial.print("[INIT] WiFi连接: ");
  setStatusLED("working");
  initWiFi();
  Serial.println("✓ 完成");
  
  Serial.print("[INIT] Web服务器: ");
  initWebServer();
  Serial.println("✓ 完成");
  

  
  Serial.print("[INIT] 统计管理器: ");
  statisticsManager.loadStatistics();
  Serial.println("✓ 完成");
  
  Serial.print("[INIT] 看门狗: ");
  watchdogManager.initWatchdog();
  Serial.println("✓ 完成");
  
  Serial.print("[INIT] 系统状态: ");
  systemStatus.initStatus();
  Serial.println("✓ 完成");
  
  logManager.addLog(LOG_INFO, "SYSTEM", "SMS转发器启动完成");
  logManager.addLog(LOG_INFO, "WIFI", "WiFi状态: " + String(WiFi.status()));
  logManager.addLog(LOG_INFO, "WEB", "Web服务器已启动");
  
  setStatusLED("ready");
  Serial.println("SMS转发器启动完成");
}

void loop() {
  // 立即喂狗避免超时
  static bool firstRun = true;
  if (firstRun) {
    watchdogManager.feedWatchdog();
    firstRun = false;
  }
  
  // 主循环处理
  server.handleClient();
  handleUartRx();  // 处理SIM7670G串口数据
  simTask();       // SIM7670G状态机
  // SMS处理已集成到simTask()中
  checkBatteryStatus();
  systemStatus.updateStatus(); // 更新系统状态缓存
  
  // 定期任务
  static unsigned long lastCheck = 0;
  static unsigned long lastWatchdog = 0;
  static unsigned long lastDailyReport = 0;
  static unsigned long lastWeeklyReport = 0;
  
  unsigned long now = millis();
  
  // 每5秒喂一次看门狗
  if (now - lastWatchdog > 5000) {
    watchdogManager.feedWatchdog();
    lastWatchdog = now;
  }
  
  if (now - lastCheck > 60000) {
    // 长短信清理已集成到sms_handler中
    memoryManager.optimizeMemory();
    
    // 检查日报
    if (config.reporting.dailyReportEnabled) {
      unsigned long nextDaily = lastDailyReport + 24UL * 60UL * 60UL * 1000UL;
      int currentHour = (millis() / (1000UL * 60UL * 60UL)) % 24;
      if (currentHour == config.reporting.reportHour && now - lastDailyReport > 23UL * 60UL * 60UL * 1000UL) {
        sendDailyReport();
        lastDailyReport = now;
      } else if (lastDailyReport == 0) {
        lastDailyReport = now;
      }
    }
    
    // 检查周报（ 每周同一小时触发，间隔>=6天 ）
    if (config.reporting.weeklyReportEnabled) {
      int currentHour = (millis() / (1000UL * 60UL * 60UL)) % 24;
      bool dueHour = currentHour == config.reporting.reportHour;
      if (dueHour && (millis() - lastWeeklyReport > 6UL * 24UL * 60UL * 60UL * 1000UL)) {
        sendWeeklyReport();
        lastWeeklyReport = millis();
      } else if (lastWeeklyReport == 0) {
        lastWeeklyReport = millis();
      }
    }
    
    lastCheck = now;
  }
  
  // 重试处理
  retryManager.processRetries();
  updateSystemLED();
  sleepManager.checkSleepCondition();
  
  delay(100);
}

void sendDailyReport() {
  Statistics stats = statisticsManager.getStatistics();
  
  String message = "每日统计报告\n";
  message += "收到短信: " + String(stats.totalSMSReceived) + "\n";
  message += "转发成功: " + String(stats.totalSMSForwarded) + "\n";
  message += "推送成功: " + String(stats.totalPushSuccess) + "\n";
  message += "推送失败: " + String(stats.totalPushFailed) + "\n";
  message += "运行时间: " + String(stats.uptime / 3600) + "小时";
  
  notificationManager.forwardSMS("系统报告", message);
  logManager.addLog(LOG_INFO, "REPORT", "发送每日报告");
}

void sendWeeklyReport() {
  Statistics stats = statisticsManager.getStatistics();
  
  String message = "每周统计报告\n";
  message += "收到短信: " + String(stats.totalSMSReceived) + "\n";
  message += "转发成功: " + String(stats.totalSMSForwarded) + "\n";
  message += "过滤短信: " + String(stats.totalSMSFiltered) + "\n";
  message += "推送成功: " + String(stats.totalPushSuccess) + "\n";
  message += "推送失败: " + String(stats.totalPushFailed) + "\n";
  message += "运行天数: " + String(stats.uptime / 86400.0, 1);
  
  notificationManager.forwardSMS("系统周报", message);
  logManager.addLog(LOG_INFO, "REPORT", "发送每周报告");
}
