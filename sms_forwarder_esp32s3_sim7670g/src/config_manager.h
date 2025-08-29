#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <SPIFFS.h>

struct Config {
  struct {
    String ssid;
    String password;
  } wifi;
  
  struct {
    bool enabled;
    String key;
    String url;
  } bark;
  
  struct {
    bool enabled;
    String key;
    String url;
  } serverChan;
  
  struct {
    bool enabled;
    String token;
    String chatId;
    String url;
  } telegram;
  
  struct {
    bool enabled;
    String webhook;
  } dingtalk;
  
  struct {
    bool enabled;
    String webhook;
  } feishu;
  
  struct {
    bool enabled;
    String url;
    String key;
  } custom;
  
  struct {
    int lowThreshold;
    int criticalThreshold;
    bool alertEnabled;
    bool chargingAlertEnabled;
    bool lowBatteryAlertEnabled;
    bool fullChargeAlertEnabled;
  } battery;
  
  struct {
    bool enabled;
    int timeout;
    int mode;
  } sleep;
  
  struct {
    bool roamingAlertEnabled;
    bool autoDisableDataRoaming;
    int signalCheckInterval;
    int operatorMode;    // 运营商模式: 0=自动, 1=移动, 2=联通, 3=电信
    String apn;          // APN设置
    String apnUser;      // APN用户名
    String apnPass;      // APN密码
  } network;
  

  
  struct {
    bool whitelistEnabled;
    bool keywordFilterEnabled;
    String whitelist;
    String blockedKeywords;
  } smsFilter;
  
  struct {
    bool dailyReportEnabled;
    bool weeklyReportEnabled;
    int reportHour;
  } reporting;
  
  struct {
    bool atCommandEcho;
  } debug;
};

extern Config config;

void initConfig();
void saveConfig();
void loadConfig();
void setDefaultConfig();
void parseConfigValue(const String& json, const String& key, String& value);
void parseConfigBool(const String& json, const String& key, bool& value);
void parseConfigInt(const String& json, const String& key, int& value);
String extractSection(const String& json, const String& sectionKey);

#endif