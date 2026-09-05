const fs = require('node:fs');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'battery_manager.cpp'), 'utf8');
runCpp(`
#include "${path.join(root, 'config_manager.h')}"
#include "${path.join(root, 'millis_utils.h')}"
#include <cassert>
#include <iostream>
#define LOGE(...) (void)0
Config config{};
uint32_t tick = 60001;
uint32_t millis() { return tick; }
struct BatteryInfo { bool available; float percentage; bool isCharging; bool isFullyCharged; bool isLowBattery; };
BatteryInfo battery{true, 2, false, false, true};
BatteryInfo getBatteryInfo() { return battery; }
String i18nFormat(const char* value) { return value; }
void sendLowBatteryAlert(const BatteryInfo&) { assert(false); }
void sendChargingAlert(const BatteryInfo&, const String&) { assert(false); }
struct { int sleeps = 0; void enterSleepMode(bool force) { assert(force); sleeps++; } } sleepManager;
${extractFunction(source, 'void checkBatteryStatus() {')}
int main() {
 config.battery.alertEnabled = false;
 config.battery.criticalThreshold = 5;
 checkBatteryStatus();
 assert(sleepManager.sleeps == 1);
 tick += 60001;
 battery.isCharging = true;
 checkBatteryStatus();
 assert(sleepManager.sleeps == 1);
 tick += 60001;
 battery.available = false;
 battery.isCharging = false;
 checkBatteryStatus();
 assert(sleepManager.sleeps == 1);
 std::cout << "Battery protection tests passed (production C++).\\n";
}
`);