#include "led_controller.h"
#include "battery_manager.h"
#include "config_manager.h"
#include "millis_utils.h"
#include "sim7670g_manager.h"
#include "time_manager.h"
#include <WiFi.h>

Adafruit_NeoPixel rgbLED(1, RGB_LED_PIN, NEO_RGB + NEO_KHZ800);
static String lastLedStatus = "";
static String lastLedReason = "";
static String ledOverlayStatus = "";
static unsigned long ledOverlayStartMs = 0;
static unsigned long ledOverlayDurationMs = 0;

static uint8_t configuredBrightnessValue() {
  int brightness = config.led.brightness;
  if (brightness < 1) brightness = 30;
  if (brightness > 100) brightness = 100;
  return static_cast<uint8_t>((brightness * 255 + 50) / 100);
}

static void applyConfiguredBrightness() {
  rgbLED.setBrightness(configuredBrightnessValue());
}

static void setRGBLEDOutput(uint8_t r, uint8_t g, uint8_t b, bool bypassConfig) {
  applyConfiguredBrightness();
  if (!bypassConfig && isLedOutputSuppressed()) {
    r = 0;
    g = 0;
    b = 0;
  }
  rgbLED.setPixelColor(0, rgbLED.Color(r, g, b));
  rgbLED.show();
}

static void setRGBLEDRaw(uint8_t r, uint8_t g, uint8_t b) {
  setRGBLEDOutput(r, g, b, true);
}

static void applyStatusLEDColor(const String& status) {
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

static bool isLedOverlayActive(unsigned long now) {
  if (ledOverlayStatus.isEmpty() || ledOverlayDurationMs == 0) {
    return false;
  }
  if (millisElapsed(now, ledOverlayStartMs, ledOverlayDurationMs)) {
    ledOverlayStatus = "";
    ledOverlayDurationMs = 0;
    return false;
  }
  return true;
}

void initLED() {
  rgbLED.begin();
  applyConfiguredBrightness();
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
  setRGBLEDOutput(r, g, b, false);
}

void setStatusLED(String status) {
  applyStatusLEDColor(status);
  lastLedStatus = status;
}

void setLedOverlay(const String& status, unsigned long durationMs) {
  if (status.isEmpty() || durationMs == 0) return;
  ledOverlayStatus = status;
  ledOverlayStartMs = millis();
  ledOverlayDurationMs = durationMs;
}

void blinkRGBLED(uint8_t r, uint8_t g, uint8_t b, int times, int interval) {
  for (int i = 0; i < times; i++) {
    setRGBLEDRaw(r, g, b);
    delay(interval);
    setRGBLEDRaw(0, 0, 0);
    delay(interval);
  }
}

bool isLedQuietHoursActive() {
  if (!config.led.quietHoursEnabled) return false;
  if (config.led.quietStartMinutes == config.led.quietEndMinutes) return false;

  int nowMinutes = getConfiguredLocalMinuteOfDay();
  if (nowMinutes < 0) return false;

  int start = config.led.quietStartMinutes;
  int end = config.led.quietEndMinutes;
  if (start < end) {
    return nowMinutes >= start && nowMinutes < end;
  }
  return nowMinutes >= start || nowMinutes < end;
}

bool isLedOutputSuppressed() {
  return !config.led.enabled || isLedQuietHoursActive();
}

void updateSystemLED() {
  static String lastStatus = "";
  static unsigned long lastUpdate = 0;
  static unsigned long lastApply = 0;
  static unsigned long startupMs = 0;
  static unsigned long lastSmsOkMs = 0;
  static unsigned long lastApBlinkMs = 0;
  static unsigned long lastReadyBlinkMs = 0;
  static unsigned long chargingLedBoostStartMs = 0;
  static bool chargingLedBoostEnabled = false;
  static float lastBatteryVoltage = -1.0f;
  static unsigned long lastBatterySampleMs = 0;
  static bool apBlinkOn = false;
  static bool readyBlinkOn = false;
  static bool overlayWasActive = false;
  const unsigned long kPowerPlugDetectWindowMs = 8000UL;
  const unsigned long kChargingLedBoostMs = 35000UL;
  const float kPowerPlugRiseThresholdV = 0.05f;
  const float kPowerUnplugDropThresholdV = -0.05f;
  
  WiFiMode_t wifiMode = WiFi.getMode();
  bool apMode = (wifiMode == WIFI_AP || wifiMode == WIFI_AP_STA);
  unsigned long now = millis();
  unsigned long updateIntervalMs = apMode ? 1000 : 1000;
  if (!millisElapsed(now, lastUpdate, updateIntervalMs)) {
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

  if (!simReady) {
    lastSmsOkMs = now;
  } else if (smsOk) {
    lastSmsOkMs = now;
  }
  
  bool simInitTimeout = !simReady && millisElapsed(now, startupMs, 180000UL);
  bool smsTimeout = simReady && !smsOk && millisElapsed(now, lastSmsOkMs, 300000UL);
  bool errorState = simInitTimeout || smsTimeout;

  if (chargingLedBoostEnabled &&
      millisElapsed(now, chargingLedBoostStartMs, kChargingLedBoostMs)) {
    chargingLedBoostEnabled = false;
  }

  if (lastBatterySampleMs != 0) {
    unsigned long sampleDt = millisSince(now, lastBatterySampleMs);
    if (sampleDt <= kPowerPlugDetectWindowMs && lastBatteryVoltage > 0.0f) {
      float deltaV = battery.voltage - lastBatteryVoltage;
      if (!battery.isCharging && !chargingLedBoostEnabled && deltaV >= kPowerPlugRiseThresholdV) {
        chargingLedBoostStartMs = now;
        chargingLedBoostEnabled = true;
      } else if (deltaV <= kPowerUnplugDropThresholdV) {
        chargingLedBoostEnabled = false;
      }
    }
  }
  lastBatteryVoltage = battery.voltage;
  lastBatterySampleMs = now;
  bool chargingLedBoostActive = chargingLedBoostEnabled &&
                                !millisElapsed(now, chargingLedBoostStartMs, kChargingLedBoostMs);
  bool showChargingLed = battery.isCharging || chargingLedBoostActive;
  
  // 优先级判断
  if (errorState) {
    currentStatus = "error";
    lastLedReason = simInitTimeout ? "SIM_INIT_TIMEOUT" : "SMS_NETWORK_TIMEOUT";
  } else if (showChargingLed) {
    currentStatus = "charging";
    lastLedReason = "CHARGING";
  } else if (battery.percentage < lowThreshold) {
    currentStatus = "low_battery";
    lastLedReason = "LOW_BATTERY";
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

  lastLedStatus = currentStatus;

  if (isLedOverlayActive(now)) {
    applyStatusLEDColor(ledOverlayStatus);
    lastStatus = currentStatus;
    overlayWasActive = true;
    return;
  }

  if (overlayWasActive) {
    overlayWasActive = false;
    lastStatus = "";
  }
  
  // 状态变化或周期性刷新时更新LED，避免被其他模块覆盖后长期停留
  if (currentStatus == "ap") {
    if (millisElapsed(now, lastApBlinkMs, updateIntervalMs)) {
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
    if (millisElapsed(now, lastReadyBlinkMs, updateIntervalMs)) {
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

  if (currentStatus != lastStatus || millisElapsed(now, lastApply, 10000UL)) {
    applyStatusLEDColor(currentStatus);
    lastStatus = currentStatus;
    lastLedStatus = currentStatus;
    lastApply = now;
  }
}

// LED硬件测试函数
void testLEDHardware() {
  setRGBLEDRaw(255, 0, 0);   delay(500);  // 红
  setRGBLEDRaw(0, 255, 0);   delay(500);  // 绿
  setRGBLEDRaw(0, 0, 255);   delay(500);  // 蓝
  setRGBLEDRaw(255, 255, 255); delay(500); // 白
  setRGBLEDRaw(0, 0, 0);     delay(500);  // 关
}

// LED状态测试函数
void testAllLEDStates() {
  setRGBLEDRaw(0, 0, 255);       delay(1000);
  setRGBLEDRaw(0, 255, 0);       delay(1000);
  setRGBLEDRaw(255, 255, 0);     delay(1000);
  setRGBLEDRaw(255, 0, 0);       delay(1000);
  setRGBLEDRaw(255, 100, 0);     delay(1000);
  setRGBLEDRaw(0, 255, 255);     delay(1000);
  setRGBLEDRaw(0, 0, 0);         delay(1000);
  updateSystemLED();
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
