#include "sim7670g_manager.h"
#include "log_manager.h"
#include "config_manager.h"
#include "watchdog_manager.h"
// 短信处理函数在sms_handler.cpp中定义

// 短信读取状态变量
bool waitingForSMSRead = false;
int currentSMSIndex = 0;
String smsReadBuffer = "";
static bool manualATInProgress = false;

// CMGL超时处理变量
unsigned long cmglStartTime = 0;
const unsigned long CMGL_TIMEOUT = 5000; // 5秒超时
bool cmglReceiving = false;
const int MAX_SMS_BUFFER_SIZE = 10240;

// 手动查询独立状态
bool manualCMGLMode = false;
String manualCMGLBuffer = "";
unsigned long manualCMGLStartTime = 0;
bool manualCMGLReceiving = false;

// 手动CMGR轮询状态
bool manualCMGRMode = false;
int totalSMSCount = 0;
int currentCMGRIndex = 0;
int foundSMSCount = 0;
int maxSMSIndex = 49; // SIM卡最大索引

// 短信合并处理变量
bool pendingSMSProcessing = false;
unsigned long firstSMSTime = 0;
const unsigned long SMS_MERGE_DELAY = 5000; // 5秒延迟
int pendingSMSIndexes[10]; // 待处理的短信索引数组，最多10条
int pendingSMSCount = 0; // 待处理短信数量

// CMT PDU处理变量
static int expectedPDULenChars = 0;  // PDU字符串长度（字符数 = bytes*2）
static bool awaitingCmtPdu = false;

// 系统状态管理
SystemStatusManager systemStatus;
SystemStatus SystemStatusManager::status;
static const unsigned long STATUS_QUERY_TIMEOUT_MS = 1000;

HardwareSerial sim7670g(1);

// 状态机变量
SimState simState = SIM_STATE_IDLE;
unsigned long stateStartMs = 0;
unsigned long cmdSendMs = 0;
int initCmdIndex = 0;
int atRetryCount = 0;
int cmdRetryCount = 0;
bool waitingForResponse = false;
String rxBuffer = "";

const char *initCmds[] = {
  "AT",                        // 模块响应
  "ATE0",                      // 关闭回显
  "AT+CMEE=2",                 // 启用详细错误报告
  "AT+CFUN=1",                 // 全功能模式，射频开启
  "AT+CPIN?",                  // 确认SIM卡READY
  "AT+CREG=2",                 // 启用CS域注册状态URC
  "AT+CGREG=2",                // 启用GPRS注册状态URC
  "AT+CEREG=2",                // 启用LTE注册状态URC
  "AT+CSQ",                    // 信号检查
  "AT+CREG?",                  // 查询CS域注册状态
  "AT+CGREG?",                 // 查询GPRS注册状态
  "AT+CEREG?",                 // 查询LTE注册状态
  "AT+CMGF=0",                 // 短信PDU模式
  "AT+CNMI=2,2,0,0,0"          // 新短信URC提示
};
const int INIT_CMD_COUNT = sizeof(initCmds) / sizeof(initCmds[0]);

static bool parseRegStatFromResponse(const String& response, const String& prefix, int& statOut) {
  int idx = response.indexOf(prefix);
  if (idx < 0) return false;
  int colon = response.indexOf(':', idx);
  if (colon < 0) return false;
  String fields = response.substring(colon + 1);
  fields.trim();
  int comma = fields.indexOf(',');
  String statStr;
  if (comma >= 0) {
    statStr = fields.substring(comma + 1);
  } else {
    statStr = fields;
  }
  int nextComma = statStr.indexOf(',');
  if (nextComma >= 0) statStr = statStr.substring(0, nextComma);
  statStr.trim();
  if (statStr.isEmpty()) return false;
  statOut = statStr.toInt();
  return true;
}

static String mapOperatorName(const String& op) {
  String code = op;
  code.trim();
  if (code.length() == 5 || code.length() == 6) {
    bool numeric = true;
    for (int i = 0; i < code.length(); i++) {
      if (code.charAt(i) < '0' || code.charAt(i) > '9') {
        numeric = false;
        break;
      }
    }
    if (numeric) {
      if (code == "46000" || code == "46002" || code == "46007" || code == "46004") return "中国移动";
      if (code == "46001" || code == "46006" || code == "46009") return "中国联通";
      if (code == "46003" || code == "46005" || code == "46011") return "中国电信";
      if (code == "23410") return "giffgaff";
    }
  }
  return op;
}

void changeState(SimState newState) {
  String stateNames[] = {"IDLE", "POWER_ON", "WAIT_BOOT", "WAIT_AT_OK", "INIT_CMDS", "CONFIG_APN", "READY"};
  logManager.addLog(LOG_DEBUG, "STATE", "State: " + stateNames[simState] + " -> " + stateNames[newState]);
  simState = newState;
  stateStartMs = millis();
}

void powerPulsePwrKey() {
  digitalWrite(SIM7670G_RESET_PIN, LOW);
  delay(100);
  digitalWrite(SIM7670G_RESET_PIN, HIGH);
  delay(500);
  
  digitalWrite(SIM7670G_PWR_PIN, HIGH);
  delay(100);
  digitalWrite(SIM7670G_PWR_PIN, LOW);
  delay(1000);
  digitalWrite(SIM7670G_PWR_PIN, HIGH);
  
  watchdogManager.feedWatchdog(); // 只在结束时喂一次狗
  logManager.addLog(LOG_INFO, "SIM7670G", "电源控制完成");
}

void sendAT(const char *cmd) {
  if (config.debug.atCommandEcho) {
    logManager.addLog(LOG_DEBUG, "AT_TX", cmd);
  }
  
  sim7670g.println(cmd);
  sim7670g.flush();
  
  cmdSendMs = millis();
  waitingForResponse = true;
  
  delay(100);
  handleUartRx();
}

void processLine(String line) {
  line.trim();
  if (line.length() == 0) return;
  
  // 输出所有串口返回消息
  logManager.addLog(LOG_DEBUG, "AT_RX", line);
  
  // 处理短信读取响应
  if (waitingForSMSRead) {
    // 防止缓冲区溢出
    if (smsReadBuffer.length() + line.length() + 1 < MAX_SMS_BUFFER_SIZE) {
      smsReadBuffer += line + "\n";
    } else {
      logManager.addLog(LOG_ERROR, "SMS_BUFFER", "短信缓冲区溢出，强制处理");
      waitingForSMSRead = false;
      if (currentSMSIndex == -1) {
        extern void processCMGLResponse(const String& response);
        processCMGLResponse(smsReadBuffer);
      }
      smsReadBuffer = "";
      return;
    }
    
    if (currentSMSIndex == -1) {
      // CMGL模式：使用超时机制
      
      if (line.startsWith("+CMGL:")) {
        if (!cmglReceiving) {
          cmglReceiving = true;
          logManager.addLog(LOG_DEBUG, "SMS_CMGL", "开始接收CMGL数据");
        }
        cmglStartTime = millis(); // 重置计时器
        logManager.addLog(LOG_DEBUG, "SMS_CMGL", "CMGL数据: " + line);
      } else if (cmglReceiving && !line.isEmpty() && line != "OK") {
        // 短信内容行或其他CMGL相关数据
        cmglStartTime = millis(); // 重置计时器
        logManager.addLog(LOG_DEBUG, "SMS_CMGL", "CMGL内容: " + line);
      } else if (line == "ERROR") {
        waitingForSMSRead = false;
        cmglReceiving = false;
        logManager.addLog(LOG_ERROR, "SMS_CMGL", "CMGL查询失败");
        smsReadBuffer = "";
      } else if (line == "OK" && !cmglReceiving) {
        // 直接收到OK，说明没有短信
        waitingForSMSRead = false;
        logManager.addLog(LOG_INFO, "SMS_CMGL", "没有短信需要处理");
        smsReadBuffer = "";
      }
      return; // 重要：防止继续处理其他逻辑
    } else if (currentSMSIndex == -2) {
      // CMT模式：直接短信内容
      if (!line.isEmpty() && !line.startsWith("+") && !line.equals("OK")) {
        smsReadBuffer += line + "\n";
        
        // 处理完整的CMT短信，但不立即处理，等待5秒合并
        waitingForSMSRead = false;
        extern void storePendingCMTSMS(const String& cmtData);
        storePendingCMTSMS(smsReadBuffer);
        smsReadBuffer = "";
      }
      return;
    } else {
      // CMGR模式：单条短信
      if (line == "OK" || line == "ERROR") {
        waitingForSMSRead = false;
        
        if (smsReadBuffer.indexOf("+CMGR:") >= 0) {
          foundSMSCount++;
          logManager.addLog(LOG_DEBUG, "SMS_CMGR", "找到短信索引 " + String(currentSMSIndex) + "，已找到 " + String(foundSMSCount) + "/" + String(totalSMSCount));
          
          if (manualCMGRMode) {
            extern void storeTempSMSFromCMGR(const String& rawData, int smsIndex);
            storeTempSMSFromCMGR(smsReadBuffer, currentSMSIndex);
          } else {
            extern void handleRawSMSData(const String& rawData, int smsIndex);
            handleRawSMSData(smsReadBuffer, currentSMSIndex);
          }
        } else if (line == "ERROR") {
          logManager.addLog(LOG_DEBUG, "SMS_CMGR", "索引 " + String(currentSMSIndex) + " 读取失败");
        } else {
          logManager.addLog(LOG_DEBUG, "SMS_CMGR", "索引 " + String(currentSMSIndex) + " 为空");
        }
        
        if (manualCMGRMode) {
          currentCMGRIndex++;
          
          if (foundSMSCount >= totalSMSCount || currentCMGRIndex >= 49) {
            logManager.addLog(LOG_INFO, "SMS_MANUAL", "CMGR轮询完成，找到 " + String(foundSMSCount) + " 条短信");
            manualCMGRMode = false;
            
            extern void processBatchedSMS();
            processBatchedSMS();
          } else {
            logManager.addLog(LOG_DEBUG, "SMS_MANUAL", "继续读取索引 " + String(currentCMGRIndex));
            readSMSByIndex(currentCMGRIndex);
          }
        }
        
        smsReadBuffer = "";
      }
    }
  }
  
  // 处理手动CMGL查询
  if (manualCMGLMode) {
    manualCMGLBuffer += line + "\n";
    
    if (line.startsWith("+CMGL:")) {
      if (!manualCMGLReceiving) {
        manualCMGLReceiving = true;
        logManager.addLog(LOG_DEBUG, "SMS_MANUAL", "开始接收手动CMGL数据");
      }
      manualCMGLStartTime = millis();
    } else if (manualCMGLReceiving && !line.isEmpty() && line != "OK") {
      manualCMGLStartTime = millis();
    } else if (line == "OK") {
      manualCMGLMode = false;
      logManager.addLog(LOG_INFO, "SMS_MANUAL", "手动CMGL查询完成，开始处理数据");
      extern void processCMGLResponse(const String& response);
      processCMGLResponse(manualCMGLBuffer);
      manualCMGLBuffer = "";
    } else if (line == "ERROR") {
      manualCMGLMode = false;
      logManager.addLog(LOG_ERROR, "SMS_MANUAL", "手动CMGL查询失败");
      manualCMGLBuffer = "";
    }
    return;
  }
  
  // 处理短信列表查询响应（只在非CMGL等待模式下）
  if (line.startsWith("+CMGL:") && !waitingForSMSRead && !manualCMGLMode) {
    logManager.addLog(LOG_INFO, "SMS_LIST", "发现短信: " + line);
    
    // 解析索引并读取短信
    int commaPos = line.indexOf(',');
    if (commaPos > 0) {
      String indexStr = line.substring(7, commaPos);
      indexStr.trim();
      int smsIndex = indexStr.toInt();
      if (smsIndex > 0) {
        logManager.addLog(LOG_INFO, "SMS_PROCESS", "处理短信索引: " + String(smsIndex));
        readSMSByIndex(smsIndex);
      }
    }
    return;
  }
  
  // 处理新短信通知
  if (line.startsWith("+CMTI")) {
    logManager.addLog(LOG_INFO, "SMS_NOTIFY", "收到短信通知: " + line);
    
    if (simState != SIM_STATE_READY) {
      logManager.addLog(LOG_WARN, "SMS", "SIM未就绪，跳过短信处理");
      return;
    }
    
    // 解析短信索引
    int commaPos = line.lastIndexOf(',');
    if (commaPos <= 0) return;
    
    String indexStr = line.substring(commaPos + 1);
    indexStr.trim();
    int smsIndex = indexStr.toInt();
    if (smsIndex <= 0) return;
    
    // 如果已在处理短信，添加到待处理数组
    if (waitingForSMSRead || pendingSMSProcessing) {
      logManager.addLog(LOG_INFO, "SMS", "正在处理短信，添加索引 " + String(smsIndex) + " 到待处理数组");
      // 检查是否已存在，避免重复
      bool exists = false;
      for (int i = 0; i < pendingSMSCount; i++) {
        if (pendingSMSIndexes[i] == smsIndex) {
          exists = true;
          break;
        }
      }
      if (!exists && pendingSMSCount < 10) {
        pendingSMSIndexes[pendingSMSCount++] = smsIndex;
      }
      firstSMSTime = millis(); // 重置等待时间
      pendingSMSProcessing = true;
      return;
    }
    
    // 第一条短信通知，开始等待期
    logManager.addLog(LOG_INFO, "SMS", "收到第一条短信通知（索引 " + String(smsIndex) + "），等待5秒看是否有更多短信");
    pendingSMSProcessing = true;
    firstSMSTime = millis();
    pendingSMSCount = 0;
    pendingSMSIndexes[pendingSMSCount++] = smsIndex;
    
    return;
  }
  
  // 处理CMT PDU第二行（纯hex数据）
  if (awaitingCmtPdu) {
    awaitingCmtPdu = false; // 取完第二行就清掉等待状态
    String pduLine = line;
    pduLine.trim();
    
    // 使用新的PDU长度验证函数
    extern bool validatePduLength(const String &pduHex, int tpduLength);
    int tpduLength = expectedPDULenChars / 2; // 转换为字节数
    if (expectedPDULenChars > 0 && !validatePduLength(pduLine, tpduLength)) {
      logManager.addLog(LOG_WARN, "CMT_PARSE", "PDU长度验证失败: TPDU=" + 
                        String(tpduLength) + "字节, 实际=" + 
                        String(pduLine.length()) + "字符");
      // 仍然处理数据，但记录警告
    }
    
    // 存进待处理队列（只存纯HEX，不带+CMT行）
    extern void storePendingCMTSMS(const String &pduHex);
    storePendingCMTSMS(pduLine);
    logManager.addLog(LOG_DEBUG, "SMS_CMT", "存储CMT短信，待处理数量: " + String(pendingSMSCount + 1));
    return;
  }
  
  // 处理直接短信内容通知
  if (line.startsWith("+CMT:")) {
    logManager.addLog(LOG_INFO, "SMS_CMT", "收到直接短信: " + line);
    
    if (simState != SIM_STATE_READY) {
      logManager.addLog(LOG_WARN, "SMS", "SIM未就绪，跳过短信处理");
      return;
    }
    
    // 解析 <length>，计算期望的字符数
    int commaPos = line.lastIndexOf(',');
    if (commaPos > 0) {
      String lengthStr = line.substring(commaPos + 1);
      lengthStr.trim();
      int lengthBytes = lengthStr.toInt();
      expectedPDULenChars = lengthBytes * 2;
      awaitingCmtPdu = true;
      logManager.addLog(LOG_DEBUG, "CMT_PARSE", "解析CMT长度: TPDU=" + String(lengthBytes) + "字节");
    } else {
      // 尝试从简化格式解析: +CMT: "",152
      int spacePos = line.lastIndexOf(' ');
      if (spacePos > 0) {
        String lengthStr = line.substring(spacePos + 1);
        lengthStr.trim();
        int lengthBytes = lengthStr.toInt();
        if (lengthBytes > 0) {
          expectedPDULenChars = lengthBytes * 2;
          awaitingCmtPdu = true;
          logManager.addLog(LOG_DEBUG, "CMT_PARSE", "从简化格式解析PDU长度: " + String(lengthBytes) + "字节");
        } else {
          expectedPDULenChars = 0;
          awaitingCmtPdu = false;
        }
      } else {
        expectedPDULenChars = 0;
        awaitingCmtPdu = false;
      }
    }
    
    // CMT的第一行不存，只记录等待状态
    if (!pendingSMSProcessing) {
      logManager.addLog(LOG_INFO, "SMS", "收到第一条CMT短信，等待5秒看是否有更多短信");
      pendingSMSProcessing = true;
      firstSMSTime = millis();
      pendingSMSCount = 0;
    } else {
      firstSMSTime = millis(); // 重置等待时间
    }
    return;
  }
  
  // 处理OK响应
  if (line == "OK") {
    if (config.debug.atCommandEcho) {
      Serial.println("[OK_RESPONSE] State=" + String(simState) + ", waitingForResponse=" + String(waitingForResponse));
    }
    waitingForResponse = false;
    cmdRetryCount = 0;
    
    if (simState == SIM_STATE_WAIT_AT_OK) {
      atRetryCount = 0;
      if (config.debug.atCommandEcho) {
        Serial.println("[STATE_CHANGE] WAIT_AT_OK -> INIT_CMDS");
      }
      changeState(SIM_STATE_INIT_CMDS);
    } else if (simState == SIM_STATE_INIT_CMDS) {
      logManager.addLog(LOG_DEBUG, "INIT_OK", "指令成功: " + String(initCmds[initCmdIndex]));
      initCmdIndex++;
      if (config.debug.atCommandEcho) {
        Serial.println("[INIT_CMD] Completed cmd " + String(initCmdIndex-1) + ", next: " + String(initCmdIndex));
      }
      if (initCmdIndex >= INIT_CMD_COUNT) {
        logManager.addLog(LOG_INFO, "SIM7670G", "初始化完成，配置网络");
        changeState(SIM_STATE_CONFIG_APN);
        sendNetworkConfig();
      }
    } else if (simState == SIM_STATE_CONFIG_APN) {
      logManager.addLog(LOG_INFO, "SIM7670G", "网络配置完成，模块就绪");
      changeState(SIM_STATE_READY);
    }
    return;
  }
  
  // 处理ERROR响应
  if (line == "ERROR" || line.startsWith("+CME ERROR") || line.startsWith("+CMS ERROR")) {
    if (line.indexOf("Last PDN disconnection not allowed") >= 0 &&
        config.network.dataPolicy == DATA_POLICY_ALWAYS_OFF) {
      logManager.addLog(LOG_WARN, "NET_CFG", "忽略CGACT断开限制: " + line);
      waitingForResponse = false;
      cmdRetryCount = 0;
      if (simState == SIM_STATE_CONFIG_APN) {
        changeState(SIM_STATE_READY);
      }
      return;
    }

    waitingForResponse = false;
    logManager.addLog(LOG_WARN, "AT_ERROR", "指令错误: " + line);
    
    if (simState == SIM_STATE_WAIT_AT_OK) {
      // AT测试失败，增加重试计数
      atRetryCount++;
      logManager.addLog(LOG_DEBUG, "AT_ERROR", "AT测试失败，重试次数: " + String(atRetryCount));
    } else if (simState == SIM_STATE_INIT_CMDS) {
      // 初始化指令失败，重试当前指令
      logManager.addLog(LOG_WARN, "INIT_ERROR", "指令失败: " + String(initCmds[initCmdIndex]) + " - " + line);
      cmdRetryCount++;
      if (cmdRetryCount > 3) {
        logManager.addLog(LOG_ERROR, "INIT", "指令重试失败，跳过: " + String(initCmds[initCmdIndex]));
        initCmdIndex++;
        cmdRetryCount = 0;
      }
    } else if (simState == SIM_STATE_CONFIG_APN) {
      // 网络配置失败，重试
      cmdRetryCount++;
    }
    return;
  }
  
  if (line.indexOf("READY") >= 0 && simState == SIM_STATE_INIT_CMDS) {
    logManager.addLog(LOG_INFO, "SIM", "SIM卡就绪");
  }
  
  // 状态解析已移动到系统状态管理器
  // 只保留IP地址和PING响应的日志
  if (line.indexOf("+CGPADDR:") >= 0) {
    logManager.addLog(LOG_INFO, "IP", "获得IP地址: " + line);
  }
  
  if (line.indexOf("+CPING:") >= 0) {
    if (line.indexOf(",3,") >= 0) {
      logManager.addLog(LOG_INFO, "PING", "网络连通性测试完成");
    } else {
      logManager.addLog(LOG_DEBUG, "PING", line);
    }
  }
  
  // 处理CPMS响应（手动查询模式）
  if (line.indexOf("+CPMS:") >= 0) {
    // 解析 +CPMS: "SM",12,50,"SM",12,50,"SM",12,50
    int firstComma = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    
    if (firstComma > 0 && secondComma > 0) {
      totalSMSCount = line.substring(firstComma + 1, secondComma).toInt();
      
      // 获取最大容量
      int thirdComma = line.indexOf(',', secondComma + 1);
      if (thirdComma > 0) {
        maxSMSIndex = line.substring(secondComma + 1, thirdComma).toInt();
      }
      
      logManager.addLog(LOG_INFO, "SMS_MANUAL", "发现 " + String(totalSMSCount) + " 条短信，容量 " + String(maxSMSIndex));
      
      if (totalSMSCount > 0) {
        // 开始手动CMGR轮询
        manualCMGRMode = true;
        currentCMGRIndex = 0;
        foundSMSCount = 0;
        extern void clearTempSMSStorage();
        clearTempSMSStorage();
        
        logManager.addLog(LOG_INFO, "SMS_MANUAL", "开始CMGR轮询，从索引0到49");
        // 读取第一条短信
        readSMSByIndex(currentCMGRIndex);
      } else {
        logManager.addLog(LOG_INFO, "SMS_MANUAL", "没有短信需要处理");
      }
    }
    return;
  }
  

}

void handleUartRx() {
  while (sim7670g.available()) {
    char c = sim7670g.read();
    rxBuffer += c;
    
    if (c == '\n' || c == '\r') {
      if (rxBuffer.length() > 1) {
        String line = rxBuffer;
        line.trim();
        rxBuffer = "";
        
        if (line.length() > 0) {
          processLine(line);
        }
      } else {
        rxBuffer = "";
      }
    }
    
    if (rxBuffer.length() > 10240) {
      rxBuffer = "";
    }
  }
}

void initSIM7670G() {
  pinMode(SIM7670G_PWR_PIN, OUTPUT);
  pinMode(SIM7670G_RESET_PIN, OUTPUT);
  
  sim7670g.begin(115200, SERIAL_8N1, SIM7670G_RX_PIN, SIM7670G_TX_PIN);
  
  while (sim7670g.available()) {
    sim7670g.read();
  }
  
  if (config.debug.atCommandEcho) {
    Serial.println("[INIT] 串口初始化完成: RX=" + String(SIM7670G_RX_PIN) + ", TX=" + String(SIM7670G_TX_PIN));
  }
  
  logManager.addLog(LOG_INFO, "SIM7670G", "开始上电");
  powerPulsePwrKey();
  changeState(SIM_STATE_WAIT_BOOT);
}

void simTask() {
  switch (simState) {
    case SIM_STATE_IDLE:
      // 空闲状态，等待初始化
      break;
      
    case SIM_STATE_WAIT_BOOT:
      if (millis() - stateStartMs > 20000) {  // 20秒启动时间
        logManager.addLog(LOG_INFO, "SIM7670G", "开始AT测试");
        // 清空串口缓冲区
        while (sim7670g.available()) {
          sim7670g.read();
        }
        changeState(SIM_STATE_WAIT_AT_OK);
        atRetryCount = 0;
        sendAT("AT");
      }
      break;
      
    case SIM_STATE_WAIT_AT_OK:
      // 检查超时或错误重试
      if ((!waitingForResponse && atRetryCount > 0) || (waitingForResponse && millis() - cmdSendMs > 3000)) {
        atRetryCount++;
        if (atRetryCount > 15) {
          logManager.addLog(LOG_ERROR, "SIM7670G", "AT测试失败，重新启动模块");
          atRetryCount = 0;
          changeState(SIM_STATE_IDLE);
          delay(2000);
          initSIM7670G();
          return;
        }
        logManager.addLog(LOG_DEBUG, "AT_RETRY", "AT测试重试: " + String(atRetryCount));
        // 清空串口缓冲区
        while (sim7670g.available()) {
          sim7670g.read();
        }
        delay(500);  // 稍微等待
        sendAT("AT");
      }
      break;
      
    case SIM_STATE_INIT_CMDS:
      if (initCmdIndex < INIT_CMD_COUNT) {
        if (!waitingForResponse) {
          // 发送下一条指令
          sendAT(initCmds[initCmdIndex]);
        } else if (millis() - cmdSendMs > 3000) {
          // 指令超时
          cmdRetryCount++;
          if (cmdRetryCount > 3) {
            logManager.addLog(LOG_ERROR, "INIT", "指令超时，跳过: " + String(initCmds[initCmdIndex]));
            initCmdIndex++;
            cmdRetryCount = 0;
            waitingForResponse = false;
          } else {
            logManager.addLog(LOG_WARN, "INIT", "指令超时重试: " + String(initCmds[initCmdIndex]));
            sendAT(initCmds[initCmdIndex]);
          }
        }
      } else {
        // 所有初始化指令完成
        logManager.addLog(LOG_INFO, "SIM7670G", "初始化完成，配置网络");
        changeState(SIM_STATE_CONFIG_APN);
        sendNetworkConfig();
      }
      break;
      
    case SIM_STATE_CONFIG_APN:
      if (waitingForResponse && millis() - cmdSendMs > 5000) {
        cmdRetryCount++;
        if (cmdRetryCount > 3) {
          logManager.addLog(LOG_ERROR, "NET_CFG", "网络配置失败，进入就绪状态");
          changeState(SIM_STATE_READY);
        } else {
          logManager.addLog(LOG_WARN, "NET_CFG", "网络配置超时，重试");
          sendNetworkConfig();
        }
      }
      break;
      
    case SIM_STATE_READY:
      // 模块就绪，更新系统状态
      {
        static unsigned long lastStatusUpdate = 0;
        static unsigned long lastSMSCheck = 0;
        
        if (millis() - lastStatusUpdate > 30000) { // 30秒更新一次
          systemStatus.updateStatus();
          lastStatusUpdate = millis();
        }
        
        // 定期检查短信通知配置
        if (millis() - lastSMSCheck > 30000) { // 30秒检查一次
          checkSMSNotificationConfig();
          lastSMSCheck = millis();
        }
        
        // 检查CMGL超时
        if (waitingForSMSRead && currentSMSIndex == -1 && cmglReceiving && 
            millis() - cmglStartTime >= CMGL_TIMEOUT) {
          logManager.addLog(LOG_INFO, "SMS_CMGL", "CMGL接收超时，开始处理已收集的数据");
          waitingForSMSRead = false;
          cmglReceiving = false;
          extern void processCMGLResponse(const String& response);
          processCMGLResponse(smsReadBuffer);
          smsReadBuffer = "";
        }
        
        // 检查手动CMGL超时
        if (manualCMGLMode && manualCMGLReceiving && 
            millis() - manualCMGLStartTime >= CMGL_TIMEOUT) {
          logManager.addLog(LOG_INFO, "SMS_MANUAL", "手动CMGL接收超时，开始处理数据");
          manualCMGLMode = false;
          extern void processCMGLResponse(const String& response);
          processCMGLResponse(manualCMGLBuffer);
          manualCMGLBuffer = "";
        }
        
        // 检查是否需要处理待处理的短信
        if (pendingSMSProcessing && millis() - firstSMSTime >= SMS_MERGE_DELAY) {
          logManager.addLog(LOG_INFO, "SMS", "5秒等待结束，开始处理短信");
          pendingSMSProcessing = false;
          
          // 处理CMTI索引短信
          for (int i = 0; i < pendingSMSCount; i++) {
            if (!waitingForSMSRead) {
              readSMSByIndex(pendingSMSIndexes[i]);
              delay(100);
            }
          }
          pendingSMSCount = 0;
          
          // 处理CMT短信
          extern void processPendingCMTSMS();
          processPendingCMTSMS();
        }
      }
      break;
      
    default:
      logManager.addLog(LOG_ERROR, "STATE", "未知状态: " + String(simState));
      changeState(SIM_STATE_IDLE);
      break;
  }
  
  // 定期输出状态信息用于调试
  static unsigned long lastDebugOutput = 0;
  if (millis() - lastDebugOutput > 10000) { // 每10秒输出一次
    String stateNames[] = {"IDLE", "POWER_ON", "WAIT_BOOT", "WAIT_AT_OK", "INIT_CMDS", "CONFIG_APN", "READY"};
    logManager.addLog(LOG_INFO, "SIM_STATUS", "State: " + stateNames[simState] + ", Ready: " + String(simState == SIM_STATE_READY ? "YES" : "NO"));
    lastDebugOutput = millis();
  }
}

void powerOnSIM7670G() {
  digitalWrite(SIM7670G_PWR_PIN, LOW);
  delay(1000);
  digitalWrite(SIM7670G_PWR_PIN, HIGH);
  delay(2000);
  Serial.println("SIM7670G电源控制完成");
}

String sendATCommand(const String& command) {
  if (manualATInProgress) {
    return "BUSY: AT in progress";
  }
  if (simState != SIM_STATE_READY) {
    return "ERROR: SIM not ready";
  }
  if (waitingForResponse || waitingForSMSRead || manualCMGLMode || manualCMGRMode) {
    return "BUSY: modem busy";
  }

  manualATInProgress = true;
  logManager.addLog(LOG_INFO, "WEB_AT", "发送AT指令: " + command);
  
  sim7670g.println(command);
  sim7670g.flush();
  
  // 简单等待响应
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 3000) {
    if (sim7670g.available()) {
      response += sim7670g.readString();
      break;
    }
    delay(10);
  }
  
  logManager.addLog(LOG_INFO, "WEB_AT", "响应: " + response);
  manualATInProgress = false;
  if (response.isEmpty()) {
    return "NO RESPONSE";
  }
  return response;
}

String getATCommandDescription(const String& command) {
  if (command == "AT") return "基础连接测试";
  if (command == "AT+CPIN?") return "检查SIM卡状态";
  if (command == "AT+CMGF=0") return "设置短信PDU模式";
  if (command == "AT+CMGF=1") return "设置短信文本模式（不推荐）";
  if (command == "AT+CFUN=0") return "关闭射频";
  if (command == "AT+CFUN=1") return "打开全功能";
  if (command == "AT+CFUN=1,1") return "开启全功能并重启";
  if (command.startsWith("AT+QCFG=\"nwscanmode\"")) return "设置网络扫描模式";
  if (command.startsWith("AT+QCFG=\"nwscanseq\"")) return "设置扫描顺序";
  if (command.startsWith("AT+QCFG=\"iotopmode\"")) return "设置IoT操作模式";
  if (command == "AT+COPS=0") return "自动选择运营商";
  if (command == "AT+CNMI=2,1,0,0,0") return "新短信到达立即通知";
  if (command == "AT+CSCS=\"GSM\"") return "字符集";
  if (command == "AT+CMEE=2") return "启用详细错误报告";
  if (command == "AT+CREG=2") return "启用CS域注册状态URC";
  if (command == "AT+CGREG=2") return "启用GPRS注册状态URC";
  if (command == "AT+CEREG=2") return "启用LTE注册状态URC";
  if (command == "AT+CGREG?") return "查询GPRS注册状态";
  if (command == "AT+CEREG?") return "查询4G注册状态";
  if (command.startsWith("AT+CNMP=")) return "设置网络模式";
  if (command.startsWith("AT+CGDCONT=")) return "定义PDP上下文";
  if (command == "AT+CGACT=1,1") return "激活PDP上下文";
  if (command == "AT+CREG?") return "查询网络注册状态";
  if (command == "AT+CSQ") return "获取信号强度";
  if (command == "AT+COPS?") return "查询运营商信息";
  if (command == "AT+CGMI") return "获取制造商信息";
  if (command == "AT+CGMM") return "获取模块型号";
  if (command == "AT+CGMR") return "获取固件版本";
  if (command == "AT+CGSN") return "获取IMEI号码";
  if (command == "AT+CCID") return "获取SIM卡ICCID";
  if (command == "AT+CIMI") return "获取IMSI号码";
  if (command.startsWith("AT+CMGS=")) return "发送短信";
  if (command.startsWith("AT+CMGR=")) return "读取短信";
  if (command == "AT+CMGL=4") return "列出所有短信";
  if (command.startsWith("AT+CMGD=")) return "删除短信";
  if (command == "AT+CPMS?") return "查询短信存储状态";
  if (command == "AT+CGATT?") return "查询GPRS附着状态";
  if (command == "AT+CGACT?") return "查询PDP上下文状态";
  return "执行AT指令";
}



// 状态查询函数已移动到系统状态管理器
void resetSIMCheck() {
  logManager.addLog(LOG_INFO, "SIM", "重置SIM检测状态");
}

// 检查短信通知配置
void checkSMSNotificationConfig() {
  if (simState != SIM_STATE_READY) return;
  
  logManager.addLog(LOG_DEBUG, "SMS_CFG", "检查短信通知配置");
  sim7670g.println("AT+CNMI?");
  sim7670g.flush();
}

// 手动查询所有短信
void checkAllSMS() {
  if (simState != SIM_STATE_READY || waitingForSMSRead || manualCMGLMode || manualCMGRMode) return;
  
  logManager.addLog(LOG_INFO, "SMS_MANUAL", "开始手动查询所有短信");
  
  // 使用CPMS查询短信数量
  sim7670g.println("AT+CPMS?");
  sim7670g.flush();
}

// 网络配置指令
void sendNetworkConfig() {
  // 设置运营商选择
  String operatorCmd;
  String defaultApn = "CMNET";
  int dataPolicy = config.network.dataPolicy;
  bool enableData = true;
  int radioMode = config.network.radioMode;

  if (radioMode > 0) {
    String modeCmd = "AT+CNMP=" + String(radioMode);
    logManager.addLog(LOG_INFO, "NET_CFG", "设置网络制式: " + modeCmd);
    sendAT(modeCmd.c_str());
  }
  switch (config.network.operatorMode) {
    case 0: // 自动选网
      operatorCmd = "AT+COPS=0";
      logManager.addLog(LOG_INFO, "NET_CFG", "自动选网");
      break;
    case 1: // 中国移动
      operatorCmd = "AT+COPS=1,2,\"46000\",7";
      logManager.addLog(LOG_INFO, "NET_CFG", "锁定中国移动LTE");
      break;
    case 2: // 中国联通
      operatorCmd = "AT+COPS=1,2,\"46001\",7";
      logManager.addLog(LOG_INFO, "NET_CFG", "锁定中国联通LTE");
      defaultApn = "3gnet";
      break;
    case 3: // 中国电信
      operatorCmd = "AT+COPS=1,2,\"46003\",7";
      logManager.addLog(LOG_INFO, "NET_CFG", "锁定中国电信LTE");
      defaultApn = "ctnet";
      break;
    case 4: // 英国 giffgaff (O2)
      operatorCmd = "AT+COPS=0";
      logManager.addLog(LOG_INFO, "NET_CFG", "giffgaff 预设APN，自动选网");
      defaultApn = "giffgaff.com";
      break;
    default:
      operatorCmd = "AT+COPS=0";
      break;
  }
  
  // 设置APN
  sendAT(operatorCmd.c_str());
  
  String apn = config.network.apn.isEmpty() ? defaultApn : config.network.apn;
  logManager.addLog(LOG_INFO, "NET_CFG", "配置网络: " + operatorCmd + ", APN: " + apn);

  if (dataPolicy == DATA_POLICY_ALWAYS_OFF) {
    enableData = false;
  } else {
    enableData = true;
  }

  if (enableData) {
    String apnCmd = "AT+CGDCONT=1,\"IP\",\"" + apn + "\"";
    sendAT(apnCmd.c_str());
    
    // 3. 激活PDP上下文
    logManager.addLog(LOG_INFO, "NET_CFG", "激活PDP上下文");
    sendAT("AT+CGACT=1,1");
    
    // 4. 获取IP地址
    sendAT("AT+CGPADDR=1");
  } else {
    logManager.addLog(LOG_INFO, "NET_CFG", "数据策略: 禁用移动数据");
    sendAT("AT+CGACT=0,1");
    sendAT("AT+CGATT=0");
  }
}

// 网络测试已移动到系统状态管理器

// 读取指定索引的短信
void readSMSByIndex(int index) {
  logManager.addLog(LOG_INFO, "SMS_READ", "开始读取短信索引: " + String(index));
  
  // 使用非阻塞方式发送读取指令
  String readCmd = "AT+CMGR=" + String(index);
  if (config.debug.atCommandEcho) {
    logManager.addLog(LOG_DEBUG, "AT_TX", readCmd);
  }
  
  sim7670g.println(readCmd);
  sim7670g.flush();
  
  // 设置等待短信读取响应的标志
  waitingForSMSRead = true;
  currentSMSIndex = index;
  smsReadBuffer = "";
}

// 发送短信（文本模式）
bool sendSMS(const String& phoneNumber, const String& message) {
  logManager.addLog(LOG_INFO, "SMS_SEND", "发送短信到: " + phoneNumber);
  
  if (simState != SIM_STATE_READY) {
    logManager.addLog(LOG_ERROR, "SMS_SEND", "SIM模块未就绪");
    return false;
  }
  
  // 设置文本模式
  sim7670g.println("AT+CMGF=1");
  sim7670g.flush();
  delay(500);
  
  // 设置字符集
  sim7670g.println("AT+CSCS=\"GSM\"");
  sim7670g.flush();
  delay(500);
  
  // 发送CMGS命令
  String cmgsCmd = "AT+CMGS=\"" + phoneNumber + "\"";
  sim7670g.println(cmgsCmd);
  sim7670g.flush();
  
  // 等待>提示符
  unsigned long startTime = millis();
  bool gotPrompt = false;
  while (millis() - startTime < 5000) {
    if (sim7670g.available()) {
      String response = sim7670g.readString();
      if (response.indexOf(">") >= 0) {
        gotPrompt = true;
        break;
      }
      if (response.indexOf("ERROR") >= 0) {
        logManager.addLog(LOG_ERROR, "SMS_SEND", "CMGS命令失败: " + response);
        // 恢复PDU模式
        sim7670g.println("AT+CMGF=0");
        return false;
      }
    }
    delay(10);
  }
  
  if (!gotPrompt) {
    logManager.addLog(LOG_ERROR, "SMS_SEND", "等待>提示符超时");
    // 恢复PDU模式
    sim7670g.println("AT+CMGF=0");
    return false;
  }
  
  // 发送短信内容
  sim7670g.print(message);
  sim7670g.write(0x1A); // Ctrl+Z
  sim7670g.flush();
  
  // 等待发送结果
  startTime = millis();
  bool success = false;
  while (millis() - startTime < 30000) {
    if (sim7670g.available()) {
      String response = sim7670g.readString();
      if (response.indexOf("+CMGS:") >= 0) {
        logManager.addLog(LOG_INFO, "SMS_SEND", "短信发送成功");
        success = true;
        break;
      }
      if (response.indexOf("ERROR") >= 0) {
        logManager.addLog(LOG_ERROR, "SMS_SEND", "短信发送失败: " + response);
        break;
      }
    }
    watchdogManager.feedWatchdog();
    delay(100);
  }
  
  if (!success && millis() - startTime >= 30000) {
    logManager.addLog(LOG_ERROR, "SMS_SEND", "短信发送超时");
  }
  
  // 恢复PDU模式
  sim7670g.println("AT+CMGF=0");
  sim7670g.flush();
  
  return success;
}

// ========== 系统状态管理功能 ==========

void SystemStatusManager::initStatus() {
  logManager.addLog(LOG_INFO, "STATUS", "初始化系统状态缓存");
  
  status.signalStrength = -999;
  status.simReady = false;
  status.networkConnected = false;
  status.csRegistered = false;
  status.epsRegistered = false;
  status.dataAttached = false;
  status.operatorName = "Unknown";
  status.networkType = "Unknown";
  status.isRoaming = false;
  status.lastUpdate = 0;
  status.initialized = true;
  
  if (simState == SIM_STATE_READY) {
    queryAllStatus();
  }
}

void SystemStatusManager::updateStatus() {
  if (simState != SIM_STATE_READY) {
    status.simReady = false;
    status.networkConnected = false;
    status.csRegistered = false;
    status.epsRegistered = false;
    status.dataAttached = false;
    status.signalStrength = -999;
    return;
  }
  
  if (needsUpdate()) {
    queryAllStatus();
    status.lastUpdate = millis();
  }
}

void SystemStatusManager::refreshSignalOnly() {
  if (simState == SIM_STATE_READY) {
    querySignalStrength();
  }
}

void SystemStatusManager::refreshAllStatus() {
  if (simState == SIM_STATE_READY) {
    queryAllStatus();
  } else {
    status.simReady = false;
    status.networkConnected = false;
    status.csRegistered = false;
    status.epsRegistered = false;
    status.dataAttached = false;
    status.signalStrength = -999;
    status.lastUpdate = millis();
  }
}

SystemStatus SystemStatusManager::getStatus() {
  return status;
}

bool SystemStatusManager::needsUpdate() {
  if (!status.initialized) return true;
  
  // 使用配置的信号检查间隔，默认30秒
  unsigned long interval = (config.network.signalCheckInterval > 0) ? 
                          config.network.signalCheckInterval * 1000 : 30000;
  return (millis() - status.lastUpdate) > interval;
}

void SystemStatusManager::queryAllStatus() {
  if (simState != SIM_STATE_READY) {
    if (config.debug.atCommandEcho) {
      logManager.addLog(LOG_DEBUG, "STATUS", "SIM模块未就绪，跳过状态查询");
    }
    return;
  }
  
  querySignalStrength();
  querySIMStatus();
  queryNetworkStatus();
  queryDataStatus();
  queryOperatorInfo();
  
  status.lastUpdate = millis();
  
  if (config.debug.atCommandEcho) {
    logManager.addLog(LOG_DEBUG, "STATUS", 
      "状态更新: 信号=" + String(status.signalStrength) + 
      "dBm, 网络=" + status.operatorName + 
      "(" + status.networkType + ")");
  }
}

void SystemStatusManager::querySignalStrength() {
  if (simState != SIM_STATE_READY) return;
  
  sim7670g.println("AT+CSQ");
  sim7670g.flush();
  
  // 等待响应并解析
  unsigned long startTime = millis();
  while (millis() - startTime < STATUS_QUERY_TIMEOUT_MS) {
    if (sim7670g.available()) {
      String response = sim7670g.readString();
      if (response.indexOf("+CSQ:") >= 0) {
        int start = response.indexOf("+CSQ: ") + 6;
        int comma = response.indexOf(',', start);
        if (comma > start) {
          int rssi = response.substring(start, comma).toInt();
          if (rssi != 99) {
            status.signalStrength = -113 + (rssi * 2);
          } else {
            status.signalStrength = -999;
          }
        }
      }
      break;
    }
    delay(10);
  }
}

void SystemStatusManager::querySIMStatus() {
  status.simReady = (simState == SIM_STATE_READY);
}

void SystemStatusManager::queryNetworkStatus() {
  if (simState != SIM_STATE_READY) {
    status.networkConnected = false;
    status.csRegistered = false;
    status.epsRegistered = false;
    status.isRoaming = false;
    return;
  }
  
  int epsStat = -1;
  int csStat = -1;
  bool gotEps = false;
  bool gotCs = false;

  sim7670g.println("AT+CEREG?");
  sim7670g.flush();
  
  unsigned long startTime = millis();
  while (millis() - startTime < STATUS_QUERY_TIMEOUT_MS) {
    if (sim7670g.available()) {
      String response = sim7670g.readString();
      if (parseRegStatFromResponse(response, "+CEREG:", epsStat)) {
        gotEps = true;
      }
      break;
    }
    delay(10);
  }

  sim7670g.println("AT+CREG?");
  sim7670g.flush();
  
  startTime = millis();
  while (millis() - startTime < STATUS_QUERY_TIMEOUT_MS) {
    if (sim7670g.available()) {
      String response = sim7670g.readString();
      if (parseRegStatFromResponse(response, "+CREG:", csStat)) {
        gotCs = true;
      }
      break;
    }
    delay(10);
  }

  status.epsRegistered = gotEps && (epsStat == 1 || epsStat == 5);
  status.csRegistered = gotCs && (csStat == 1 || csStat == 5);
  status.networkConnected = status.epsRegistered || status.csRegistered;

  if (gotEps) {
    status.isRoaming = (epsStat == 5);
  } else if (gotCs) {
    status.isRoaming = (csStat == 5);
  } else {
    status.isRoaming = false;
  }
}

void SystemStatusManager::queryDataStatus() {
  if (simState != SIM_STATE_READY) {
    status.dataAttached = false;
    return;
  }
  
  sim7670g.println("AT+CGATT?");
  sim7670g.flush();
  
  unsigned long startTime = millis();
  while (millis() - startTime < STATUS_QUERY_TIMEOUT_MS) {
    if (sim7670g.available()) {
      String response = sim7670g.readString();
      int idx = response.indexOf("+CGATT:");
      if (idx >= 0) {
        int colon = response.indexOf(':', idx);
        if (colon >= 0) {
          String val = response.substring(colon + 1);
          val.trim();
          int comma = val.indexOf(',');
          if (comma >= 0) {
            val = val.substring(0, comma);
          }
          val.trim();
          status.dataAttached = (val.toInt() == 1);
        }
      }
      break;
    }
    delay(10);
  }
}

void SystemStatusManager::queryOperatorInfo() {
  if (simState != SIM_STATE_READY) {
    status.operatorName = "Unknown";
    status.networkType = "Unknown";
    return;
  }
  
  sim7670g.println("AT+COPS?");
  sim7670g.flush();
  
  unsigned long startTime = millis();
  while (millis() - startTime < STATUS_QUERY_TIMEOUT_MS) {
    if (sim7670g.available()) {
      String response = sim7670g.readString();
      if (response.indexOf("+COPS:") >= 0) {
        // 解析运营商名称
        int nameStart = response.indexOf('"');
        int nameEnd = response.indexOf('"', nameStart + 1);
        if (nameStart >= 0 && nameEnd > nameStart) {
          status.operatorName = mapOperatorName(response.substring(nameStart + 1, nameEnd));
        }
        
        // 解析网络类型
        if (response.indexOf(",7") >= 0) {
          status.networkType = "4G";
        } else if (response.indexOf(",2") >= 0) {
          status.networkType = "3G";
        } else {
          status.networkType = "2G";
        }
      }
      break;
    }
    delay(10);
  }
}
