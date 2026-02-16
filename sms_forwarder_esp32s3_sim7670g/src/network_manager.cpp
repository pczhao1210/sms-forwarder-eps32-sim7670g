#include "network_manager.h"
#include "config_manager.h"
#include "log_manager.h"
#include "notification_manager.h"
#include "sim7670g_manager.h"
#include "i18n.h"

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
  bool hasCurrent = (!status.operatorCode.isEmpty() && status.operatorCode != "Unknown");
  bool hasHome = (!status.homeOperatorCode.isEmpty() && status.homeOperatorCode != "Unknown");
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
  LOGI("NETWORK", "network_init");
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
      LOGW("NETWORK", "network_roaming_detected");
      
      pending_roaming_alert = config.network.roamingAlertEnabled;
      pending_roaming_since_ms = now;
      
      if (allowRoamingDataControl) {
        if (!data_suspended_for_roaming) {
          setDataConnection(false);
          data_suspended_for_roaming = true;
          LOGI("ROAM", "roam_data_disabled");
        }
      }
    } else {
      LOGI("NETWORK", "network_roaming_end");
      pending_roaming_alert = false;
      if (data_suspended_for_roaming && allowRoamingDataControl) {
        if (shouldEnableDataForPolicy(sysStatus)) {
          setDataConnection(true);
          LOGI("ROAM", "roam_data_restored");
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
        LOGI("ROAM", "roam_alert_recent_skip");
      }
      pending_roaming_alert = false;
    }
  }
  
  return isRoaming;
}

void SMSNetworkManager::sendRoamingAlert(const NetworkInfo& network) {
  String message = i18nFormat("roam_alert_title");
  message += "\n";
  if (!network.home_operator_name.isEmpty() && network.home_operator_name != "Unknown" &&
      !network.operator_name.isEmpty() && network.operator_name != "Unknown") {
    message += i18nFormat("roam_alert_line_sim", network.home_operator_name.c_str(), network.operator_name.c_str());
    message += "\n";
  } else if (!network.operator_name.isEmpty() && network.operator_name != "Unknown") {
    message += i18nFormat("roam_alert_line_current", network.operator_name.c_str());
    message += "\n";
  }
  if (!network.operator_code.isEmpty() && network.operator_code != "Unknown") {
    message += i18nFormat("roam_alert_line_operator_code", network.operator_code.c_str());
    message += "\n";
  }
  if (!network.home_network.isEmpty() && network.home_network != "Unknown") {
    message += i18nFormat("roam_alert_line_home_code", network.home_network.c_str());
    message += "\n";
  }
  message += i18nFormat("roam_alert_line_signal", String(network.signal_strength).c_str());
  message += "\n";
  message += i18nFormat("roam_alert_footer");
  
  notificationManager.forwardSMS(i18nFormat("roam_alert_title"), message);
  LOGW("ROAM", "roam_alert_sent");
}

void SMSNetworkManager::setDataConnection(bool enable) {
  if (enable == data_connection_enabled) {
    LOGD("DATA", "data_state_no_change", enable ? "ON" : "OFF");
    return;
  }

  if (enable && config.network.autoDisableDataRoaming) {
    SystemStatus sysStatus = systemStatus.getStatus();
    if (sysStatus.isRoaming) {
      LOGW("DATA", "data_roaming_block");
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
    LOGI("DATA", enable ? "data_enabled" : "data_disabled");
  } else {
    LOGE("DATA", "data_switch_fail", cmdResult.c_str());
  }
}

void SMSNetworkManager::diagnoseNetwork() {
  LOGI("DIAG", "diag_start");
  
  // 使用系统状态信息进行诊断，不直接发送AT命令
  SystemStatus sysStatus = systemStatus.getStatus();
  
  LOGI("DIAG", "diag_sim_status", sysStatus.simReady ? i18nGet("bool_yes") : i18nGet("bool_no"));
  LOGI("DIAG", "diag_network_connected", sysStatus.networkConnected ? i18nGet("bool_yes") : i18nGet("bool_no"));
  LOGI("DIAG", "diag_signal", String(sysStatus.signalStrength).c_str());
  LOGI("DIAG", "diag_operator", sysStatus.operatorName.c_str());
  LOGI("DIAG", "diag_network_type", sysStatus.networkType.c_str());
  LOGI("DIAG", "diag_roaming", sysStatus.isRoaming ? i18nGet("bool_yes") : i18nGet("bool_no"));
  
  LOGI("DIAG", "diag_done");
}

void SMSNetworkManager::checkNetworkStatus() {
  static unsigned long lastCheck = 0;
  unsigned long interval = config.network.signalCheckInterval * 1000;
  
  if (millis() - lastCheck < interval) return;
  lastCheck = millis();
  
  // 检查SIM模块状态
  if (simState != SIM_STATE_READY) {
    LOGD("NETWORK", "network_sim_not_ready", String(simState).c_str());
    return;
  }
  
  // 更新系统状态（这会触发必要的AT命令）
  systemStatus.updateStatus();
  
  SystemStatus sysStatus = systemStatus.getStatus();
  bool shouldEnableData = shouldEnableDataForPolicy(sysStatus);
  
  // 检查信号强度
  if (sysStatus.signalStrength == -999) {
    LOGW("NETWORK", "network_signal_unavailable");
  } else if (sysStatus.signalStrength < -100) {
    LOGW("NETWORK", "network_signal_weak", String(sysStatus.signalStrength).c_str());
  }
  
  // 检查网络连接状态
  if (!sysStatus.networkConnected) {
    LOGW("NETWORK", "network_not_connected");
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
        LOGW("DATA", "data_gprs_attach_retry");
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
    LOGE("NET_TEST", "net_test_sim_not_ready");
    return false;
  }
  
  LOGI("NET_TEST", "net_test_start");
  
  // 基于系统状态进行简单的连通性判断
  SystemStatus sysStatus = systemStatus.getStatus();
  
  if (!sysStatus.networkConnected) {
    LOGE("NET_TEST", "net_test_no_network");
    return false;
  }
  
  if (sysStatus.signalStrength < -110) {
    LOGE("NET_TEST", "net_test_signal_weak", String(sysStatus.signalStrength).c_str());
    return false;
  }
  
  LOGI("NET_TEST", "net_test_pass");
  LOGI("NET_TEST", "net_test_operator_signal", sysStatus.operatorName.c_str(), String(sysStatus.signalStrength).c_str());
  
  return true;
}
