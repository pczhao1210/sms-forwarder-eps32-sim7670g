#include "led_controller.h"
#include "battery_manager.h"
#include "config_manager.h"
#include "sim7670g_manager.h"
#include <WiFi.h>

Adafruit_NeoPixel rgbLED(1, RGB_LED_PIN, NEO_RGB + NEO_KHZ800);
static String lastLedStatus = "";
static String lastLedReason = "";

void initLED() {
  rgbLED.begin();
  rgbLED.setBrightness(77); // 30%亮度
  rgbLED.clear();
  rgbLED.show();
  
  pinMode(USER_LED_PIN, OUTPUT);
  digitalWrite(USER_LED_PIN, LOW);
  
  // 初始化测试：显示红色短暂闪烁
  setRGBLED(255, 0, 0);
  delay(1000);
  setRGBLED(0, 0, 0);
}

void setRGBLED(uint8_t r, uint8_t g, uint8_t b) {
  rgbLED.setPixelColor(0, rgbLED.Color(r, g, b));
  rgbLED.show();
}

void setStatusLED(String status) {
  if (status == "init") {
    setRGBLED(0, 0, 255);      // 蓝色 - 初始化
  } else if (status == "ready") {
    setRGBLED(0, 255, 0);      // 绿色 - 就绪
  } else if (status == "working") {
    setRGBLED(255, 255, 0);    // 黄色 - 工作中
  } else if (status == "error") {
    setRGBLED(255, 0, 0);      // 红色 - 错误
  } else if (status == "low_battery") {
    setRGBLED(255, 100, 0);    // 橙色 - 低电量
  } else if (status == "charging") {
    setRGBLED(0, 255, 255);    // 青色 - 充电中
  } else if (status == "off") {
    setRGBLED(0, 0, 0);        // 关闭
  }
}

void blinkRGBLED(uint8_t r, uint8_t g, uint8_t b, int times, int interval) {
  for (int i = 0; i < times; i++) {
    setRGBLED(r, g, b);
    delay(interval);
    setRGBLED(0, 0, 0);
    delay(interval);
  }
}

void updateSystemLED() {
  static String lastStatus = "";
  static unsigned long lastUpdate = 0;
  static unsigned long lastApply = 0;
  static unsigned long startupMs = 0;
  static unsigned long lastSmsOkMs = 0;
  static unsigned long lastApBlinkMs = 0;
  static unsigned long lastReadyBlinkMs = 0;
  static bool apBlinkOn = false;
  static bool readyBlinkOn = false;
  
  WiFiMode_t wifiMode = WiFi.getMode();
  bool apMode = (wifiMode == WIFI_AP || wifiMode == WIFI_AP_STA);
  unsigned long now = millis();
  unsigned long updateIntervalMs = apMode ? 1000 : 1000;
  if (now - lastUpdate < updateIntervalMs) {
    return;
  }
  lastUpdate = now;
  
  String currentStatus = "";
  
  if (startupMs == 0) {
    startupMs = now;
    lastSmsOkMs = startupMs;
  }

  BatteryInfo battery = getBatteryInfo();
  SystemStatus sysStatus = systemStatus.getStatus();
  bool simReady = (simState == SIM_STATE_READY);
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  float lowThreshold = (config.battery.lowThreshold > 0) ? config.battery.lowThreshold : 15;
  bool smsOk = sysStatus.networkConnected || sysStatus.csRegistered || sysStatus.epsRegistered;

  if (smsOk) {
    lastSmsOkMs = now;
  }
  
  bool simInitTimeout = !simReady && (now - startupMs > 180000UL);
  bool smsTimeout = simReady && !smsOk && (now - lastSmsOkMs > 300000UL);
  bool errorState = simInitTimeout || smsTimeout;
  
  // 优先级判断
  if (battery.percentage < lowThreshold) {
    currentStatus = "low_battery";
    lastLedReason = "LOW_BATTERY";
  } else if (errorState) {
    currentStatus = "error";
    lastLedReason = simInitTimeout ? "SIM_INIT_TIMEOUT" : "SMS_NETWORK_TIMEOUT";
  } else if (battery.isCharging) {
    currentStatus = "charging";
    lastLedReason = "CHARGING";
  } else if (apMode) {
    currentStatus = "ap";
    lastLedReason = "WIFI_AP_MODE";
  } else if (!simReady) {
    currentStatus = "working";
    lastLedReason = "SIM_NOT_READY";
  } else if (!wifiConnected) {
    currentStatus = "working";
    lastLedReason = "WIFI_NOT_CONNECTED";
  } else {
    currentStatus = "ready";
    lastLedReason = "READY";
  }
  
  // 状态变化或周期性刷新时更新LED，避免被其他模块覆盖后长期停留
  if (currentStatus == "ap") {
    if (now - lastApBlinkMs >= updateIntervalMs) {
      apBlinkOn = !apBlinkOn;
      if (apBlinkOn) {
        setRGBLED(0, 0, 255); // 蓝色闪烁 - AP模式
      } else {
        setRGBLED(0, 0, 0);
      }
      lastApBlinkMs = now;
    }
    lastStatus = currentStatus;
    lastLedStatus = currentStatus;
    lastApply = now;
    return;
  }

  if (currentStatus == "ready") {
    if (now - lastReadyBlinkMs >= updateIntervalMs) {
      readyBlinkOn = !readyBlinkOn;
      if (readyBlinkOn) {
        setRGBLED(0, 255, 0); // 绿色闪烁 - 就绪
      } else {
        setRGBLED(0, 0, 0);
      }
      lastReadyBlinkMs = now;
    }
    lastStatus = currentStatus;
    lastLedStatus = currentStatus;
    lastApply = now;
    return;
  }

  if (currentStatus != lastStatus || (now - lastApply > 10000)) {
    setStatusLED(currentStatus);
    lastStatus = currentStatus;
    lastLedStatus = currentStatus;
    lastApply = now;
  }
}

// LED硬件测试函数
void testLEDHardware() {
  setRGBLED(255, 0, 0);   delay(500);  // 红
  setRGBLED(0, 255, 0);   delay(500);  // 绿
  setRGBLED(0, 0, 255);   delay(500);  // 蓝
  setRGBLED(255, 255, 255); delay(500); // 白
  setRGBLED(0, 0, 0);     delay(500);  // 关
}

// LED状态测试函数
void testAllLEDStates() {
  setStatusLED("init");       delay(1000);
  setStatusLED("ready");      delay(1000);
  setStatusLED("working");    delay(1000);
  setStatusLED("error");      delay(1000);
  setStatusLED("low_battery"); delay(1000);
  setStatusLED("charging");   delay(1000);
  setStatusLED("off");        delay(1000);
  setStatusLED("ready");
}

// 检查网络注册状态
bool checkNetworkRegistered() {
  return (simState == SIM_STATE_READY);
}

const char* getLedStatus() {
  return lastLedStatus.c_str();
}

const char* getLedReason() {
  return lastLedReason.c_str();
}
