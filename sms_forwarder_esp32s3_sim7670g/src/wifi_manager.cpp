#include "wifi_manager.h"
#include "config_manager.h"
#include "log_manager.h"

void initWiFi() {
  // 添加延迟确保系统稳定
  delay(100);
  
  // 先断开所有连接
  WiFi.disconnect(true);
  delay(100);
  
  WiFi.mode(WIFI_STA);
  delay(100);
  
  if (config.wifi.ssid.length() > 0 && config.wifi.ssid != "SMS-Forwarder-Setup") {
    connectWiFi();
  } else {
    createAP();
  }
}

void connectWiFi() {
  // 检查SSID有效性
  if (config.wifi.ssid.length() == 0 || config.wifi.ssid.length() > 32) {
    logManager.addLog(LOG_ERROR, "WIFI", "SSID无效，创建AP");
    createAP();
    return;
  }
  
  logManager.addLog(LOG_INFO, "WIFI", "连接WiFi: " + config.wifi.ssid);
  
  // 安全的WiFi连接
  WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    delay(1000);
    attempts++;
    
    // 避免看门狗复位
    if (attempts % 5 == 0) {
      yield();
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    logManager.addLog(LOG_INFO, "WIFI", "连接成功，IP: " + WiFi.localIP().toString());
  } else {
    logManager.addLog(LOG_ERROR, "WIFI", "连接失败，创建AP");
    createAP();
  }
}

void createAP() {
  // 先断开所有连接
  WiFi.disconnect(true);
  delay(100);
  
  WiFi.mode(WIFI_AP);
  delay(100);
  
  bool apStarted = WiFi.softAP("SMS-Forwarder-Setup", "12345678");
  
  if (apStarted) {
    Serial.println("已启动AP模式");
    Serial.println("SSID: SMS-Forwarder-Setup");
    Serial.println("Password: 12345678");
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    
    logManager.addLog(LOG_INFO, "WIFI", "AP模式，IP: " + WiFi.softAPIP().toString());
  } else {
    logManager.addLog(LOG_ERROR, "WIFI", "AP创建失败");
  }
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String getWiFiStatusText(wl_status_t status) {
  switch(status) {
    case WL_IDLE_STATUS: return "空闲";
    case WL_NO_SSID_AVAIL: return "SSID不可用";
    case WL_SCAN_COMPLETED: return "扫描完成";
    case WL_CONNECTED: return "已连接";
    case WL_CONNECT_FAILED: return "连接失败";
    case WL_CONNECTION_LOST: return "连接丢失";
    case WL_DISCONNECTED: return "已断开";
    default: return "未知状态";
  }
}

void diagnoseWiFi() {
  logManager.addLog(LOG_INFO, "WIFI", "WiFi诊断开始");
  
  wl_status_t status = WiFi.status();
  logManager.addLog(LOG_INFO, "WIFI", "WiFi状态: " + getWiFiStatusText(status));
  
  if (status == WL_CONNECTED) {
    logManager.addLog(LOG_INFO, "WIFI", "IP地址: " + WiFi.localIP().toString());
    logManager.addLog(LOG_INFO, "WIFI", "RSSI: " + String(WiFi.RSSI()) + "dBm");
    logManager.addLog(LOG_INFO, "WIFI", "BSSID: " + WiFi.BSSIDstr());
  } else {
    logManager.addLog(LOG_INFO, "WIFI", "配置SSID: " + config.wifi.ssid);
    logManager.addLog(LOG_INFO, "WIFI", "密码长度: " + String(config.wifi.password.length()));
    
    // 扫描可用网络
    int n = WiFi.scanNetworks();
    logManager.addLog(LOG_INFO, "WIFI", "扫描到 " + String(n) + " 个网络");
    
    bool ssidFound = false;
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == config.wifi.ssid) {
        ssidFound = true;
        logManager.addLog(LOG_INFO, "WIFI", "找到目标SSID: " + WiFi.SSID(i) + ", RSSI: " + String(WiFi.RSSI(i)));
        break;
      }
    }
    
    if (!ssidFound) {
      logManager.addLog(LOG_ERROR, "WIFI", "未找到目标SSID: " + config.wifi.ssid);
    }
  }
}