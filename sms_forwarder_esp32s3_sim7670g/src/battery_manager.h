#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Wire.h>

#define I2C_SDA_PIN 3
#define I2C_SCL_PIN 2

enum ChargingState {
  CHARGING_UNKNOWN,
  CHARGING_DISCHARGING,
  CHARGING_CHARGING,
  CHARGING_FULL,
  CHARGING_LOW_BATTERY
};

struct BatteryInfo {
  float voltage;
  float percentage;
  float chargeRate;
  bool isCharging;
  bool isLowBattery;
  bool isFullyCharged;
  ChargingState chargingState;
};

class SleepManager {
private:
  unsigned long lastActivity = 0;
  unsigned long sleepTimeout = 30 * 60 * 1000;
  bool sleepEnabled = true;
  uint8_t sleepMode = 1;
  
public:
  void updateActivity();
  void checkSleepCondition();
  void enterSleepMode();
  void setupWakeupSources();
  void handleWakeup();
};

extern SleepManager sleepManager;

bool initBatteryMonitor();
BatteryInfo getBatteryInfo();
void checkBatteryStatus();
void sendLowBatteryAlert(const BatteryInfo& battery);
void sendChargingAlert(const BatteryInfo& battery, const String& message);
ChargingState getChargingState(const BatteryInfo& battery);

#endif