#include "led_controller.h"
#include "battery_manager.h"
#include "sim7670g_manager.h"
#include <WiFi.h>

Adafruit_NeoPixel rgbLED(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

void initLED() {
  rgbLED.begin();
  rgbLED.setBrightness(128); // 50%亮度
  rgbLED.clear();
  rgbLED.show();
  
  pinMode(USER_LED_PIN, OUTPUT);
  digitalWrite(USER_LED_PIN, LOW);
  
  // 初始化测试：显示红色短暂闪烁
  setRGBLED(255, 0, 0);
  delay(100);
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
  String currentStatus = "";
  
  BatteryInfo battery = getBatteryInfo();
  bool simReady = (simState == SIM_STATE_READY);
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  // 优先级判断
  if (battery.percentage < 15) {
    currentStatus = "low_battery";
  } else if (battery.isCharging) {
    currentStatus = "charging";
  } else if (!simReady) {
    currentStatus = "working";
  } else if (!wifiConnected) {
    currentStatus = "working";
  } else {
    currentStatus = "ready";
  }
  
  // 只有状态变化时才更新LED
  if (currentStatus != lastStatus) {
    setStatusLED(currentStatus);
    lastStatus = currentStatus;
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