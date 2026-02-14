#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <SPIFFS.h>

enum DataPolicy {
  DATA_POLICY_ALWAYS_OFF = 0,
  DATA_POLICY_ROAMING_ONLY = 1,
  DATA_POLICY_ALWAYS_ON = 2
};

struct Config {
  struct {
    String ssid;
    String password;
    bool useCustomDns;
    bool forceStaticDns;
    String staticIp;
    String staticGateway;
    String staticSubnet;
    String dns1;
    String dns2;
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
    int radioMode;       // 网络制式: 2=自动, 38=LTE only
    String apn;          // APN设置
    String apnUser;      // APN用户名
    String apnPass;      // APN密码
    int dataPolicy;      // 移动数据策略: 0=始终禁用, 1=仅非漫游启用, 2=始终启用
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

  struct {
    int timeout;
  } watchdog;
  
  struct {
    bool enabled;
    String hostname;
    String password;
  } ota;
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
