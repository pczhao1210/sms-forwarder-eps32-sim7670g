#include "battery_manager.h"
#include "config_manager.h"
#include "notification_manager.h"
#include "log_manager.h"
#include "led_controller.h"
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
  logManager.addLog(LOG_INFO, "BATTERY", "MAX17048初始化成功");
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
  info.percentage = (soc >> 8) + (soc & 0xFF) / 256.0;
  
  // 计算充电状态
  static float lastPercentage = info.percentage;
  info.chargeRate = (info.percentage - lastPercentage) * 60; // %/小时
  lastPercentage = info.percentage;
  
  info.isCharging = info.chargeRate > 0.1;
  info.isLowBattery = info.percentage < 15.0;
  info.isFullyCharged = info.percentage > 95.0 && info.chargeRate < 0.1;
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
  
  if (millis() - lastCheck < 60000) return;
  lastCheck = millis();
  
  BatteryInfo battery = getBatteryInfo();
  
  // 低电量告警
  if (battery.isLowBattery && !lastLowState) {
    setStatusLED("low_battery");
    sendLowBatteryAlert(battery);
  } else if (battery.isCharging && !lastLowState) {
    setStatusLED("charging");
  } else if (!battery.isLowBattery && !battery.isCharging) {
    setStatusLED("ready");
  }
  
  // 极低电量保护
  if (battery.percentage < 5.0) {
    logManager.addLog(LOG_ERROR, "BATTERY", "极低电量，进入保护模式");
    sleepManager.enterSleepMode();
  }
  
  lastLowState = battery.isLowBattery;
}

void sendLowBatteryAlert(const BatteryInfo& battery) {
  String message = "低电量警告\n";
  message += "当前电量: " + String(battery.percentage, 1) + "%\n";
  message += "电池电压: " + String(battery.voltage, 2) + "V\n";
  message += "充电状态: ";
  message += battery.isCharging ? "充电中" : "未充电";
  
  notificationManager.forwardSMS("系统警告", message);
  logManager.addLog(LOG_WARN, "BATTERY", "发送低电量警告");
}

void SleepManager::updateActivity() {
  lastActivity = millis();
}

void SleepManager::checkSleepCondition() {
  if (!sleepEnabled) return;
  
  BatteryInfo battery = getBatteryInfo();
  unsigned long idleTime = millis() - lastActivity;
  
  bool shouldSleep = (idleTime > sleepTimeout) || 
                    (battery.percentage < 20.0 && idleTime > 10 * 60 * 1000);
  
  if (shouldSleep) {
    enterSleepMode();
  }
}

void SleepManager::enterSleepMode() {
  logManager.addLog(LOG_INFO, "SLEEP", "进入休眠模式");
  
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  
  setupWakeupSources();
  esp_light_sleep_start();
}

void SleepManager::setupWakeupSources() {
  esp_sleep_enable_timer_wakeup(5 * 60 * 1000000); // 5分钟
  esp_sleep_enable_uart_wakeup(1); // UART1唤醒
}