#include "battery_manager.h"
#include "config_manager.h"
#include "notification_manager.h"
#include "log_manager.h"
#include "led_controller.h"
#include "wifi_manager.h"
#include "i18n.h"
#include <WiFi.h>

SleepManager sleepManager;

// MAX17048寄存器地址
#define MAX17048_ADDR 0x36
#define VCELL_REG 0x02
#define SOC_REG 0x04
#define MODE_REG 0x06
#define VERSION_REG 0x08
#define CONFIG_REG 0x0C
#define COMMAND_REG 0xFE

// Charging heuristics
static const float kChargeRateThreshold = 0.2f;        // %/h
static const float kFullPercentThreshold = 99.0f;      // %
static const float kFullVoltageThreshold = 4.15f;      // V
static const unsigned long kFullStableMs = 10UL * 60UL * 1000UL; // 10 minutes

bool initBatteryMonitor() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  
  // 检测MAX17048
  Wire.beginTransmission(MAX17048_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("MAX17048未检测到");
    return false;
  }
  
  // 读取版本
  Wire.beginTransmission(MAX17048_ADDR);
  Wire.write(VERSION_REG);
  Wire.endTransmission();
  Wire.requestFrom(MAX17048_ADDR, 2);
  uint16_t version = (Wire.read() << 8) | Wire.read();
  
  Serial.printf("MAX17048版本: 0x%04X\n", version);
  LOGI("BATTERY", "battery_init_ok");
  return true;
}

uint16_t readRegister(uint8_t reg) {
  Wire.beginTransmission(MAX17048_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(MAX17048_ADDR, 2);
  return (Wire.read() << 8) | Wire.read();
}

BatteryInfo getBatteryInfo() {
  BatteryInfo info;
  
  // 读取电压 (1.25mV/LSB)
  uint16_t vcell = readRegister(VCELL_REG);
  info.voltage = (vcell >> 4) * 1.25 / 1000.0;
  
  // 读取电量百分比 (1/256%/LSB)
  uint16_t soc = readRegister(SOC_REG);
  float rawPercent = (soc >> 8) + (soc & 0xFF) / 256.0;
  if (rawPercent < 0.0f) rawPercent = 0.0f;
  if (rawPercent > 100.0f) rawPercent = 100.0f;
  info.percentage = rawPercent;
  
  // 计算充电状态
  static float lastPercentage = info.percentage;
  static unsigned long lastSampleMs = 0;
  static float lastRate = 0.0f;
  static unsigned long fullCandidateSinceMs = 0;
  unsigned long now = millis();
  if (lastSampleMs == 0) {
    lastSampleMs = now;
    lastPercentage = info.percentage;
    lastRate = 0.0f;
  } else {
    unsigned long dtMs = now - lastSampleMs;
    if (dtMs >= 30000) { // 至少30s间隔，减少抖动
      float dtHours = dtMs / 3600000.0f;
      float rate = (info.percentage - lastPercentage) / (dtHours > 0.0001f ? dtHours : 0.0001f);
      lastRate = lastRate * 0.7f + rate * 0.3f; // 平滑
      lastPercentage = info.percentage;
      lastSampleMs = now;
    }
  }
  info.chargeRate = lastRate;
  
  info.isCharging = (info.chargeRate > kChargeRateThreshold) && (info.percentage < kFullPercentThreshold);
  float lowThreshold = (config.battery.lowThreshold > 0) ? config.battery.lowThreshold : 15.0f;
  info.isLowBattery = info.percentage < lowThreshold;
  bool fullCandidate = (info.percentage >= kFullPercentThreshold) &&
                       (info.voltage >= kFullVoltageThreshold) &&
                       (info.chargeRate < kChargeRateThreshold);
  if (fullCandidate) {
    if (fullCandidateSinceMs == 0) {
      fullCandidateSinceMs = now;
    }
  } else {
    fullCandidateSinceMs = 0;
  }
  info.isFullyCharged = fullCandidate && (now - fullCandidateSinceMs >= kFullStableMs);
  info.chargingState = getChargingState(info);
  
  return info;
}

ChargingState getChargingState(const BatteryInfo& battery) {
  if (battery.percentage < 10.0) return CHARGING_LOW_BATTERY;
  if (battery.isFullyCharged) return CHARGING_FULL;
  if (battery.isCharging) return CHARGING_CHARGING;
  return CHARGING_DISCHARGING;
}

void checkBatteryStatus() {
  static unsigned long lastCheck = 0;
  static bool lastLowState = false;
  static bool lastChargingState = false;
  static bool lastFullState = false;
  
  if (millis() - lastCheck < 60000) return;
  lastCheck = millis();
  
  BatteryInfo battery = getBatteryInfo();
  if (!config.battery.alertEnabled) {
    lastLowState = battery.isLowBattery;
    lastChargingState = battery.isCharging;
    lastFullState = battery.isFullyCharged;
    return;
  }
  
  // 低电量告警
  if (battery.isLowBattery && !lastLowState && config.battery.lowBatteryAlertEnabled) {
    sendLowBatteryAlert(battery);
  }
  
  // 充电状态告警
  if (config.battery.chargingAlertEnabled) {
    if (battery.isCharging && !lastChargingState) {
      sendChargingAlert(battery, i18nFormat("battery_charge_started"));
    } else if (!battery.isCharging && lastChargingState) {
      sendChargingAlert(battery, i18nFormat("battery_charge_stopped"));
    }
  }
  
  // 满电告警
  if (config.battery.fullChargeAlertEnabled && battery.isFullyCharged && !lastFullState) {
    sendChargingAlert(battery, i18nFormat("battery_full"));
  }
  
  // 极低电量保护
  if (battery.percentage < 5.0) {
    LOGE("BATTERY", "battery_critical_sleep");
    sleepManager.enterSleepMode();
  }
  
  lastLowState = battery.isLowBattery;
  lastChargingState = battery.isCharging;
  lastFullState = battery.isFullyCharged;
}

void sendLowBatteryAlert(const BatteryInfo& battery) {
  String message = i18nFormat("battery_low_header");
  message += "\n";
  message += i18nFormat("battery_level_line", String(battery.percentage, 1).c_str());
  message += "\n";
  message += i18nFormat("battery_voltage_line", String(battery.voltage, 2).c_str());
  message += "\n";
  message += i18nFormat("battery_charging_line", battery.isCharging ? i18nGet("battery_charging") : i18nGet("battery_not_charging"));
  
  notificationManager.forwardSMS(i18nFormat("system_alert_title"), message);
  LOGW("BATTERY", "battery_low_alert_sent");
}

void sendChargingAlert(const BatteryInfo& battery, const String& message) {
  String payload = i18nFormat("battery_status_title");
  payload += "\n";
  payload += message;
  payload += "\n";
  payload += i18nFormat("battery_level_line", String(battery.percentage, 1).c_str());
  payload += "\n";
  payload += i18nFormat("battery_voltage_line", String(battery.voltage, 2).c_str());
  payload += "\n";
  payload += i18nFormat("battery_rate_line", String(battery.chargeRate, 2).c_str());
  payload += "\n";
  
  notificationManager.forwardSMS(i18nFormat("battery_update_title"), payload);
  LOGI("BATTERY", "battery_status_alert_sent", message.c_str());
}

void SleepManager::configure(bool enabled, unsigned long timeoutSeconds, uint8_t mode) {
  sleepEnabled = enabled && timeoutSeconds > 0;
  sleepMode = mode;
  
  if (timeoutSeconds == 0) {
    sleepTimeout = 0;
  } else {
    sleepTimeout = timeoutSeconds * 1000UL;
    if (sleepTimeout < 60000UL) {
      sleepTimeout = 60000UL; // 至少1分钟
    }
  }
  
  lastActivity = millis();
  LOGI("SLEEP", "sleep_config",
       sleepEnabled ? i18nGet("bool_true") : i18nGet("bool_false"),
       String(sleepTimeout / 1000).c_str(),
       String(sleepMode).c_str());
}

void SleepManager::updateActivity() {
  lastActivity = millis();
}

void SleepManager::checkSleepCondition() {
  static unsigned long lastCheckMs = 0;
  if (!sleepEnabled || sleepTimeout == 0) return;
  if (millis() - lastCheckMs < 5000) return;
  lastCheckMs = millis();
  
  BatteryInfo battery = getBatteryInfo();
  unsigned long idleTime = millis() - lastActivity;
  
  bool shouldSleep = (idleTime > sleepTimeout) || 
                    (battery.percentage < 20.0 && idleTime > 10 * 60 * 1000);
  
  if (shouldSleep) {
    enterSleepMode();
  }
}

void SleepManager::enterSleepMode() {
  if (!sleepEnabled) return;
  
  LOGI("SLEEP", "sleep_enter");
  
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  
  setupWakeupSources();
  
  if (sleepMode == 1) {
    esp_deep_sleep_start(); // 深度睡眠不会返回
  } else {
    esp_light_sleep_start();
    handleWakeup();
  }
}

void SleepManager::setupWakeupSources() {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup(5ULL * 60ULL * 1000000ULL); // 5分钟定时唤醒
  if (sleepMode == 0) {
    esp_sleep_enable_uart_wakeup(1); // UART1唤醒
  }
}

void SleepManager::handleWakeup() {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  LOGI("SLEEP", "sleep_wakeup_reason", String((int)cause).c_str());
  
  WiFi.mode(WIFI_STA);
  initWiFi();
  lastActivity = millis();
  LOGI("SLEEP", "sleep_resume");
}
