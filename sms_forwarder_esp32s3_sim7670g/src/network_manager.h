#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include "sim7670g_manager.h"

// 前向声明
class SystemStatusManager;
extern SystemStatusManager systemStatus;

struct NetworkInfo {
  String operator_name;
  String operator_code;
  String home_network;
  int signal_strength;
  String network_type;
  bool is_roaming;
  bool data_enabled;
};

class SMSNetworkManager {
public:
  static void initNetwork();
  static NetworkInfo getNetworkInfo();
  static void checkNetworkStatus();
  static bool detectRoaming();
  static void setDataConnection(bool enable);
  static void sendRoamingAlert(const NetworkInfo& network);
  static void diagnoseNetwork();
  static bool testConnectivity();
  
private:
  static bool last_roaming_status;
  static unsigned long last_check_time;
  static const unsigned long CHECK_INTERVAL = 300000; // 5分钟检查一次
};

extern SMSNetworkManager networkManager;

#endif