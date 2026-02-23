#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include <SPIFFS.h>

extern WebServer server;

void initWebServer();
void handleGetStatus();
void handleGetConfig();
void handleSetConfig();
void handleSetLanguage();
void handleGetBattery();
void handleDebugSystem();
void handleDebugRestart();
void handleDebugAT();
void handleDebugWiFi();
void handleDebugNetwork();
void handleDebugNotification();
void handleDebugTimeSync();
void handleGetVersion();
void handleGetStatistics();
void handleResetStatistics();
void handleGetLogs();
void handleClearLogs();
void handleSetNotificationConfig();
void handleSetBatteryConfig();
void handleSetNetworkConfig();
void handleSetSMSFilterConfig();
void handleSetSystemConfig();
void handleGetSystemInfo();
void handleGetTimeStatus();
void handleResetSIM();
void handleTestNotification();
void handleGetSMS();
void handleClearSMS();
void handleDeleteSMS();
void handleForwardSMS();
void handleGetForwardStatus();
void handleGetSystemStatus();
void handleRefreshSystemStatus();
void handleDebugEcho();
void handleDebugLED();
void handleSendSMS();
void handleCheckSMS();

#endif
