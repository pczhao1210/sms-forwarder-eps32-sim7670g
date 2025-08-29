#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Adafruit_NeoPixel.h>
#include "sim7670g_manager.h"  // 使用统一的引脚定义

extern Adafruit_NeoPixel rgbLED;

void initLED();
void setRGBLED(uint8_t r, uint8_t g, uint8_t b);
void setStatusLED(String status);
void blinkRGBLED(uint8_t r, uint8_t g, uint8_t b, int times = 3, int interval = 200);
void updateSystemLED();
void testAllLEDStates();
void testLEDHardware();
bool checkNetworkRegistered();

#endif