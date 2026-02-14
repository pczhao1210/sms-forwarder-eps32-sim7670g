#include "network_manager.h"
#include "config_manager.h"
#include "log_manager.h"
#include "notification_manager.h"
#include "sim7670g_manager.h"

SMSNetworkManager networkManager;

// 静态成员变量定义
bool SMSNetworkManager::last_roaming_status = false;
unsigned long SMSNetworkManager::last_check_time = 0;
bool SMSNetworkManager::data_connection_enabled = true;
bool SMSNetworkManager::data_suspended_for_roaming = false;
static unsigned long last_roaming_alert_ms = 0;
static bool pending_roaming_alert = false;
static unsigned long pending_roaming_since_ms = 0;

static bool hasOperatorInfo(const SystemStatus& status) {
  bool hasCurrent = (!status.operatorName.isEmpty() && status.operatorName != "Unknown") ||
                    (!status.operatorCode.isEmpty() && status.operatorCode != "Unknown");
  bool hasHome = (!status.homeOperatorName.isEmpty() && status.homeOperatorName != "Unknown") ||
                 (!status.homeOperatorCode.isEmpty() && status.homeOperatorCode != "Unknown");
  return hasCurrent && hasHome;
}

static int normalizeDataPolicy(int policy) {
  if (policy < DATA_POLICY_ALWAYS_OFF || policy > DATA_POLICY_ALWAYS_ON) {
    return DATA_POLICY_ROAMING_ONLY;
  }
  return policy;
}

static bool shouldEnableDataForPolicy(const SystemStatus& status) {
  int policy = normalizeDataPolicy(config.network.dataPolicy);
  if (config.network.autoDisableDataRoaming && status.isRoaming) return false;
  if (policy == DATA_POLICY_ALWAYS_OFF) return false;
  if (policy == DATA_POLICY_ALWAYS_ON) return true;
  return !status.isRoaming;
}

void SMSNetworkManager::initNetwork() {
  logManager.addLog(LOG_INFO, "NETWORK", "初始化网络管理");
  last_roaming_status = false;
  last_check_time = 0;
  data_connection_enabled = true;
  data_suspended_for_roaming = false;
  last_roaming_alert_ms = 0;
  pending_roaming_alert = false;
  pending_roaming_since_ms = 0;
}

NetworkInfo SMSNetworkManager::getNetworkInfo() {
  NetworkInfo info;
  
  // 完全使用系统状态管理器的缓存信息，不发送任何AT命令
  SystemStatus sysStatus = systemStatus.getStatus();
  
  info.signal_strength = sysStatus.signalStrength;
  info.operator_name = sysStatus.operatorName;
  info.home_operator_name = sysStatus.homeOperatorName;
  info.network_type = sysStatus.networkType;
  info.is_roaming = sysStatus.isRoaming;
  info.data_enabled = sysStatus.networkConnected;
  
  // 使用默认值，避免AT命令调用
  info.operator_code = sysStatus.operatorCode;
  info.home_network = sysStatus.homeOperatorCode;
  
  return info;
}

bool SMSNetworkManager::detectRoaming() {
  // 使用系统状态管理器的缓存状态，避免重复AT命令
  SystemStatus sysStatus = systemStatus.getStatus();
  bool isRoaming = sysStatus.isRoaming;
  bool allowRoamingDataControl = config.network.autoDisableDataRoaming;
  unsigned long now = millis();

  // 未注册时不触发漫游变更，避免误报
  if (!sysStatus.csRegistered && !sysStatus.epsRegistered) {
    return last_roaming_status;
  }
  
  // 只在漫游状态变化时处理
  if (isRoaming != last_roaming_status) {
    last_roaming_status = isRoaming;
    
    if (isRoaming) {
      logManager.addLog(LOG_WARN, "NETWORK", "检测到国际漫游");
      
      pending_roaming_alert = config.network.roamingAlertEnabled;
      pending_roaming_since_ms = now;
      
      if (allowRoamingDataControl) {
        if (!data_suspended_for_roaming) {
          setDataConnection(false);
          data_suspended_for_roaming = true;
          logManager.addLog(LOG_INFO, "ROAM", "漫游时自动关闭数据连接");
        }
      }
    } else {
      logManager.addLog(LOG_INFO, "NETWORK", "漫游状态结束");
      pending_roaming_alert = false;
      if (data_suspended_for_roaming && allowRoamingDataControl) {
        if (shouldEnableDataForPolicy(sysStatus)) {
          setDataConnection(true);
          logManager.addLog(LOG_INFO, "ROAM", "恢复数据连接");
        }
        data_suspended_for_roaming = false;
      }
    }
  }

  if (isRoaming && pending_roaming_alert && config.network.roamingAlertEnabled) {
    const unsigned long minDelayMs = 30000UL;
    bool infoReady = hasOperatorInfo(sysStatus);
    if (infoReady || (now - pending_roaming_since_ms) > minDelayMs) {
      if (last_roaming_alert_ms == 0 || (now - last_roaming_alert_ms) > 60000UL) {
        NetworkInfo info = getNetworkInfo();
        sendRoamingAlert(info);
        last_roaming_alert_ms = now;
      } else {
        logManager.addLog(LOG_INFO, "ROAM", "漫游告警在1分钟内已发送，跳过重复通知");
      }
      pending_roaming_alert = false;
    }
  }
  
  return isRoaming;
}

void SMSNetworkManager::sendRoamingAlert(const NetworkInfo& network) {
  String message = "漫游警告\n";
  if (!network.home_operator_name.isEmpty() && network.home_operator_name != "Unknown" &&
      !network.operator_name.isEmpty() && network.operator_name != "Unknown") {
    message += "SIM/漫游: " + network.home_operator_name + " | " + network.operator_name + "\n";
  } else {
    message += "当前网络: " + network.operator_name + "\n";
  }
  if (!network.operator_code.isEmpty() && network.operator_code != "Unknown") {
    message += "网络代码: " + network.operator_code + "\n";
  }
  if (!network.home_network.isEmpty() && network.home_network != "Unknown") {
    message += "本地网络: " + network.home_network + "\n";
  }
  message += "信号强度: " + String(network.signal_strength) + "dBm\n";
  message += "请注意漫游费用";
  
  notificationManager.forwardSMS("漫游警告", message);
  logManager.addLog(LOG_WARN, "ROAM", "发送漫游警告");
}

void SMSNetworkManager::setDataConnection(bool enable) {
  if (enable == data_connection_enabled) {
    logManager.addLog(LOG_DEBUG, "DATA", "数据连接状态无需改变: " + String(enable ? "ON" : "OFF"));
    return;
  }

  if (enable && config.network.autoDisableDataRoaming) {
    SystemStatus sysStatus = systemStatus.getStatus();
    if (sysStatus.isRoaming) {
      logManager.addLog(LOG_WARN, "DATA", "漫游中禁止开启数据连接");
      return;
    }
  }
  
  String cmdResult;
  if (!enable) {
    cmdResult = sendATCommand("AT+CGACT=0,1");
    if (cmdResult.indexOf("OK") >= 0 && config.network.dataPolicy != DATA_POLICY_ALWAYS_OFF) {
      cmdResult = sendATCommand("AT+CGATT=0");
    }
  } else {
    cmdResult = sendATCommand("AT+CGATT=1");
    if (cmdResult.indexOf("OK") >= 0) {
      cmdResult = sendATCommand("AT+CGACT=1,1");
    }
  }
  
  bool success = cmdResult.indexOf("OK") >= 0;
  if (!success && !enable && config.network.dataPolicy == DATA_POLICY_ALWAYS_OFF &&
      cmdResult.indexOf("Last PDN disconnection not allowed") >= 0) {
    success = true;
  }
  if (success) {
    data_connection_enabled = enable;
    logManager.addLog(LOG_INFO, "DATA", enable ? "数据连接已开启" : "数据连接已关闭");
  } else {
    logManager.addLog(LOG_ERROR, "DATA", "切换数据连接失败，响应: " + cmdResult);
  }
}

void SMSNetworkManager::diagnoseNetwork() {
  logManager.addLog(LOG_INFO, "DIAG", "开始网络诊断");
  
  // 使用系统状态信息进行诊断，不直接发送AT命令
  SystemStatus sysStatus = systemStatus.getStatus();
  
  logManager.addLog(LOG_INFO, "DIAG", "SIM状态: " + String(sysStatus.simReady ? "就绪" : "未就绪"));
  logManager.addLog(LOG_INFO, "DIAG", "网络连接: " + String(sysStatus.networkConnected ? "已连接" : "未连接"));
  logManager.addLog(LOG_INFO, "DIAG", "信号强度: " + String(sysStatus.signalStrength) + "dBm");
  logManager.addLog(LOG_INFO, "DIAG", "运营商: " + sysStatus.operatorName);
  logManager.addLog(LOG_INFO, "DIAG", "网络类型: " + sysStatus.networkType);
  logManager.addLog(LOG_INFO, "DIAG", "漫游状态: " + String(sysStatus.isRoaming ? "是" : "否"));
  
  logManager.addLog(LOG_INFO, "DIAG", "网络诊断完成（基于缓存状态）");
}

void SMSNetworkManager::checkNetworkStatus() {
  static unsigned long lastCheck = 0;
  unsigned long interval = config.network.signalCheckInterval * 1000;
  
  if (millis() - lastCheck < interval) return;
  lastCheck = millis();
  
  // 检查SIM模块状态
  if (simState != SIM_STATE_READY) {
    logManager.addLog(LOG_DEBUG, "NETWORK", "SIM模块未就绪，状态: " + String(simState));
    return;
  }
  
  // 更新系统状态（这会触发必要的AT命令）
  systemStatus.updateStatus();
  
  SystemStatus sysStatus = systemStatus.getStatus();
  bool shouldEnableData = shouldEnableDataForPolicy(sysStatus);
  
  // 检查信号强度
  if (sysStatus.signalStrength == -999) {
    logManager.addLog(LOG_WARN, "NETWORK", "无法获取信号强度");
  } else if (sysStatus.signalStrength < -100) {
    logManager.addLog(LOG_WARN, "NETWORK", "信号强度较弱: " + String(sysStatus.signalStrength) + "dBm");
  }
  
  // 检查网络连接状态
  if (!sysStatus.networkConnected) {
    logManager.addLog(LOG_WARN, "NETWORK", "网络未连接，尝试重新注册");
    // 只在必要时发送AT命令
    if (millis() - last_check_time > 30000) {
      sendATCommand("AT+COPS=0");
      last_check_time = millis();
    }
  }
  
  if (!shouldEnableData) {
    if (data_connection_enabled) {
      setDataConnection(false);
    }
  } else {
    // 检查数据连接（减少频率）
    static unsigned long lastDataCheck = 0;
    if (millis() - lastDataCheck > 60000) { // 1分钟检查一次
      String cgattResp = sendATCommand("AT+CGATT?");
      if (cgattResp.indexOf("+CGATT: 0") >= 0) {
        logManager.addLog(LOG_WARN, "DATA", "GPRS未附着，尝试重新附着");
        sendATCommand("AT+CGATT=1");
      }
      lastDataCheck = millis();
    }
  }
  
  // 检查漫游状态
  detectRoaming();
}

bool SMSNetworkManager::testConnectivity() {
  if (simState != SIM_STATE_READY) {
    logManager.addLog(LOG_ERROR, "NET_TEST", "SIM模块未就绪，无法测试连通性");
    return false;
  }
  
  logManager.addLog(LOG_INFO, "NET_TEST", "开始网络连通性测试");
  
  // 基于系统状态进行简单的连通性判断
  SystemStatus sysStatus = systemStatus.getStatus();
  
  if (!sysStatus.networkConnected) {
    logManager.addLog(LOG_ERROR, "NET_TEST", "网络未连接");
    return false;
  }
  
  if (sysStatus.signalStrength < -110) {
    logManager.addLog(LOG_ERROR, "NET_TEST", "信号强度过弱: " + String(sysStatus.signalStrength) + "dBm");
    return false;
  }
  
  logManager.addLog(LOG_INFO, "NET_TEST", "网络连通性检查通过（基于状态判断）");
  logManager.addLog(LOG_INFO, "NET_TEST", "运营商: " + sysStatus.operatorName + ", 信号: " + String(sysStatus.signalStrength) + "dBm");
  
  return true;
}
