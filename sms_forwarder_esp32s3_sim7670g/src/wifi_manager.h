#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

void initWiFi();
void connectWiFi();
void createAP();
bool isWiFiConnected();
String getWiFiStatusText(wl_status_t status);
void diagnoseWiFi();
void diagnoseNetwork(const String& url, const String& method, const String& payload);
void pollWiFiReconnect();

#endif
