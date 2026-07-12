#include "sim7670g_manager.h"
#include "millis_utils.h"
#include "log_manager.h"
#include "config_manager.h"
#include "watchdog_manager.h"
#include "i18n.h"
#include "operator_db.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
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
static const int MAX_PENDING_SMS_INDEXES = 10;
static const int MAX_PENDING_SMS_DELETES = 50;

// 手动查询独立状态
bool manualCMGLMode = false;
String manualCMGLBuffer = "";
unsigned long manualCMGLStartTime = 0;
bool manualCMGLReceiving = false;

// 手动CMGR轮询状态
bool manualCMGRMode = false;
int totalSMSCount = 0;
int currentCMGRIndex = 1;
int foundSMSCount = 0;
int maxSMSIndex = 50; // SIM卡最大索引（通常从1开始）

// 短信合并处理变量
bool pendingSMSProcessing = false;
unsigned long firstSMSTime = 0;
const unsigned long SMS_MERGE_DELAY = 5000; // 5秒延迟
int pendingSMSIndexes[MAX_PENDING_SMS_INDEXES]; // 待处理的短信索引数组
int pendingSMSCount = 0; // 待处理短信数量
static bool pendingSMSFullScanRequested = false;
static bool initialSMSScanRequested = false;
static int pendingSMSDeleteIndexes[MAX_PENDING_SMS_DELETES];
static int pendingSMSDeleteCount = 0;
static bool waitingForSMSDeleteResponse = false;
static int currentSMSDeleteIndex = 0;

// CMT PDU处理变量
static int expectedPDULenChars = 0;  // PDU字符串长度（字符数 = bytes*2）
static bool awaitingCmtPdu = false;

// 系统状态管理
SystemStatusManager systemStatus;
SystemStatus SystemStatusManager::status;
// 状态查询使用较短超时，避免阻塞主循环导致Web UI卡顿
static const unsigned long STATUS_QUERY_TIMEOUT_MS = 250;

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
static bool smsSending = false;
static const int MAX_NETWORK_CONFIG_CMDS = 6;
static String networkConfigCmds[MAX_NETWORK_CONFIG_CMDS];
static int networkConfigCmdCount = 0;
static int networkConfigCmdIndex = 0;
static bool networkConfigActive = false;

struct AsyncATJobState {
  bool active = false;
  bool running = false;
  bool complete = false;
  uint32_t id = 0;
  String command;
  String response;
};

struct AsyncSMSJobState {
  bool active = false;
  bool running = false;
  bool complete = false;
  bool success = false;
  uint32_t id = 0;
  String phoneNumber;
  String message;
  String error;
};

enum ModemAsyncJobType {
  MODEM_ASYNC_JOB_NONE,
  MODEM_ASYNC_JOB_AT,
  MODEM_ASYNC_JOB_SMS
};

static AsyncATJobState asyncATJob;
static AsyncSMSJobState asyncSMSJob;
static uint32_t nextModemAsyncJobId = 1;
static SemaphoreHandle_t modemAsyncJobMutex = nullptr;
static TaskHandle_t modemAsyncWorkerTaskHandle = nullptr;
static volatile bool modemAsyncWorkerActive = false;
static bool modemAsyncRecoveryScanPending = false;
static ModemAsyncJobType modemAsyncWorkerJobType = MODEM_ASYNC_JOB_NONE;

static void resetNetworkConfigQueue();
static void enqueueNetworkConfigCommand(const String& command);
static bool sendNextNetworkConfigCommand();
static bool resendCurrentNetworkConfigCommand();
static bool readNextPendingSMS();
static bool sendNextSMSDelete();
static String hexByte(uint8_t value);
static bool appendUtf16BeHexFromUtf8(const String& text, String& outputHex);
static String encodeSmsAddressSemiOctets(const String& phoneNumber, String& digitsOut, bool& internationalOut);
static bool buildSmsSubmitPdu(const String& phoneNumber, const String& message, String& pduOut, int& tpduLengthOut);
static bool waitForSmsResponse(const char* command, const char* expected, unsigned long timeoutMs, String& responseOut);
static bool waitForSmsPrompt(unsigned long timeoutMs, String& responseOut);
static bool querySmsRegistrationForSend(String& detailOut);
static void modemAsyncWorkerTask(void* parameter);

static bool lockModemAsyncJobs(TickType_t waitTicks = pdMS_TO_TICKS(100)) {
  if (!modemAsyncJobMutex) {
    modemAsyncJobMutex = xSemaphoreCreateMutex();
  }
  return modemAsyncJobMutex && xSemaphoreTake(modemAsyncJobMutex, waitTicks) == pdTRUE;
}

static void unlockModemAsyncJobs() {
  xSemaphoreGive(modemAsyncJobMutex);
}

static uint32_t allocateModemAsyncJobId() {
  uint32_t id = nextModemAsyncJobId++;
  if (nextModemAsyncJobId == 0) nextModemAsyncJobId = 1;
  return id;
}

static void requestPendingSMSFullScan(int smsIndex) {
  pendingSMSFullScanRequested = true;
  LOGW("SMS", "sms_pending_overflow", String(smsIndex).c_str(), String(MAX_PENDING_SMS_INDEXES).c_str());
}

static bool isModemBusyForStatus() {
  return waitingForResponse || waitingForSMSRead || manualCMGLMode || manualCMGRMode ||
         manualATInProgress || cmglReceiving || awaitingCmtPdu || manualCMGLReceiving ||
         manualCMGRMode || smsSending || waitingForSMSDeleteResponse;
}

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
  "AT+CNMI=2,1,0,0,0"          // 新短信存入SIM并通过+CMTI提示
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
  if (comma < 0) return false;
  String statStr = fields.substring(comma + 1);
  int nextComma = statStr.indexOf(',');
  if (nextComma >= 0) statStr = statStr.substring(0, nextComma);
  statStr.trim();
  if (statStr.isEmpty()) return false;
  for (int i = 0; i < statStr.length(); i++) {
    char c = statStr.charAt(i);
    if (c < '0' || c > '9') return false;
  }
  statOut = statStr.toInt();
  return true;
}

static String mapOperatorName(const String& op) {
  return getOperatorNameByCode(op, getCurrentLangCode());
}

static String extractDigits(const String& input) {
  String out = "";
  for (int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c >= '0' && c <= '9') out += c;
  }
  return out;
}

static String extractImsiFromResponse(const String& response) {
  String best = "";
  int start = 0;
  while (start < response.length()) {
    int end = response.indexOf('\n', start);
    if (end < 0) end = response.length();
    String line = response.substring(start, end);
    line.trim();
    String digits = extractDigits(line);
    if (digits.length() >= 10) {
      if (digits.length() > best.length()) {
        best = digits;
      }
    }
    start = end + 1;
  }
  return best;
}

static String extractHomeOperatorCodeFromImsi(const String& imsiDigits) {
  if (imsiDigits.length() < 10) return "Unknown";
  if (imsiDigits.startsWith("23410")) return "23410"; // giffgaff
  if (imsiDigits.startsWith("460") && imsiDigits.length() >= 5) {
    return imsiDigits.substring(0, 5);
  }
  if (imsiDigits.length() >= 6) {
    return imsiDigits.substring(0, 6);
  }
  return imsiDigits;
}

void changeState(SimState newState) {
  String stateNames[] = {"IDLE", "POWER_ON", "WAIT_BOOT", "WAIT_AT_OK", "INIT_CMDS", "CONFIG_APN", "READY"};
  LOGD("STATE", "sim_state_change", stateNames[simState].c_str(), stateNames[newState].c_str());
  if (newState == SIM_STATE_READY && simState != SIM_STATE_READY) {
    initialSMSScanRequested = true;
  }
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
  LOGI("SIM7670G", "sim_power_pulse_done");
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

static void resetNetworkConfigQueue() {
  for (int cmdIndex = 0; cmdIndex < MAX_NETWORK_CONFIG_CMDS; cmdIndex++) {
    networkConfigCmds[cmdIndex] = "";
  }
  networkConfigCmdCount = 0;
  networkConfigCmdIndex = 0;
  networkConfigActive = false;
}

static void enqueueNetworkConfigCommand(const String& command) {
  if (networkConfigCmdCount >= MAX_NETWORK_CONFIG_CMDS || command.isEmpty()) return;
  networkConfigCmds[networkConfigCmdCount++] = command;
}

static bool sendNextNetworkConfigCommand() {
  if (networkConfigCmdIndex >= networkConfigCmdCount) {
    networkConfigActive = false;
    return false;
  }

  networkConfigActive = true;
  String command = networkConfigCmds[networkConfigCmdIndex++];
  sendAT(command.c_str());
  return true;
}

static bool resendCurrentNetworkConfigCommand() {
  if (!networkConfigActive || networkConfigCmdIndex <= 0 || networkConfigCmdIndex > networkConfigCmdCount) {
    return false;
  }

  String command = networkConfigCmds[networkConfigCmdIndex - 1];
  sendAT(command.c_str());
  return true;
}

static String currentNetworkConfigCommand() {
  if (!networkConfigActive || networkConfigCmdIndex <= 0 || networkConfigCmdIndex > networkConfigCmdCount) {
    return "";
  }
  return networkConfigCmds[networkConfigCmdIndex - 1];
}

static bool isLikelyPduPayloadLine(const String& line) {
  if (line.length() < 32 || (line.length() % 2) != 0) return false;
  for (int charIndex = 0; charIndex < line.length(); charIndex++) {
    char value = line.charAt(charIndex);
    bool hex = (value >= '0' && value <= '9') ||
               (value >= 'A' && value <= 'F') ||
               (value >= 'a' && value <= 'f');
    if (!hex) return false;
  }
  return true;
}

void processLine(String line) {
  line.trim();
  if (line.length() == 0) return;
  
  // 输出串口返回消息，PDU载荷只记录长度，避免短信正文进入日志
  String lineForLog = line;
  if (isLikelyPduPayloadLine(line)) {
    lineForLog = "[pdu len=";
    lineForLog += String(line.length());
    lineForLog += "]";
  }
  logManager.addLog(LOG_DEBUG, "AT_RX", lineForLog);

  if (waitingForSMSDeleteResponse &&
      (line == "OK" || line == "ERROR" || line.startsWith("+CME ERROR") || line.startsWith("+CMS ERROR"))) {
    if (line != "OK") {
      logManager.addLog(LOG_WARN, "SMS_DEL", "Delete failed for index " + String(currentSMSDeleteIndex) + ": " + line);
    }
    waitingForSMSDeleteResponse = false;
    currentSMSDeleteIndex = 0;
    return;
  }
  
  // 处理短信读取响应
  if (waitingForSMSRead) {
    // 防止缓冲区溢出
    if (smsReadBuffer.length() + line.length() + 1 < MAX_SMS_BUFFER_SIZE) {
      smsReadBuffer += line + "\n";
    } else {
      LOGE("SMS_BUFFER", "sms_buffer_overflow");
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
          LOGD("SMS_CMGL", "sms_cmgl_receive_start");
        }
        cmglStartTime = millis(); // 重置计时器
        LOGD("SMS_CMGL", "sms_cmgl_line", line.c_str());
      } else if (cmglReceiving && !line.isEmpty() && line != "OK") {
        // 短信内容行或其他CMGL相关数据
        cmglStartTime = millis(); // 重置计时器
        LOGD("SMS_CMGL", "sms_cmgl_content", line.c_str());
      } else if (line == "ERROR") {
        waitingForSMSRead = false;
        cmglReceiving = false;
        LOGE("SMS_CMGL", "sms_cmgl_query_fail");
        smsReadBuffer = "";
      } else if (line == "OK" && !cmglReceiving) {
        // 直接收到OK，说明没有短信
        waitingForSMSRead = false;
        LOGI("SMS_CMGL", "sms_cmgl_no_sms");
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
          LOGD("SMS_CMGR", "sms_cmgr_found",
               String(currentSMSIndex).c_str(),
               String(foundSMSCount).c_str(),
               String(totalSMSCount).c_str());
          
          if (manualCMGRMode) {
            extern void storeTempSMSFromCMGR(const String& rawData, int smsIndex);
            storeTempSMSFromCMGR(smsReadBuffer, currentSMSIndex);
          } else {
            extern void handleRawSMSData(const String& rawData, int smsIndex);
            handleRawSMSData(smsReadBuffer, currentSMSIndex);
          }
        } else if (line == "ERROR") {
          LOGD("SMS_CMGR", "sms_cmgr_read_fail", String(currentSMSIndex).c_str());
        } else {
          LOGD("SMS_CMGR", "sms_cmgr_empty", String(currentSMSIndex).c_str());
        }
        
        if (manualCMGRMode) {
          currentCMGRIndex++;
          
          if (foundSMSCount >= totalSMSCount || currentCMGRIndex > maxSMSIndex) {
            LOGI("SMS_MANUAL", "sms_cmgr_poll_done", String(foundSMSCount).c_str());
            manualCMGRMode = false;
            
            extern void processBatchedSMS();
            processBatchedSMS();
          } else {
            LOGD("SMS_MANUAL", "sms_cmgr_poll_next", String(currentCMGRIndex).c_str());
            readSMSByIndex(currentCMGRIndex);
          }
        }
        
        smsReadBuffer = "";
      }
    }
  }
  
  // 处理手动CMGL查询
  if (manualCMGLMode) {
    if (manualCMGLBuffer.length() + line.length() + 1 >= MAX_SMS_BUFFER_SIZE) {
      LOGE("SMS_MANUAL", "sms_cmgl_manual_buffer_overflow", String(MAX_SMS_BUFFER_SIZE).c_str());
      manualCMGLMode = false;
      manualCMGLReceiving = false;
      extern void processCMGLResponse(const String& response);
      if (!manualCMGLBuffer.isEmpty()) {
        processCMGLResponse(manualCMGLBuffer);
      }
      manualCMGLBuffer = "";
      return;
    }

    manualCMGLBuffer += line + "\n";
    
    if (line.startsWith("+CMGL:")) {
      if (!manualCMGLReceiving) {
        manualCMGLReceiving = true;
        LOGD("SMS_MANUAL", "sms_cmgl_manual_start");
      }
      manualCMGLStartTime = millis();
    } else if (manualCMGLReceiving && !line.isEmpty() && line != "OK") {
      manualCMGLStartTime = millis();
    } else if (line == "OK") {
      manualCMGLMode = false;
      LOGI("SMS_MANUAL", "sms_cmgl_manual_done");
      extern void processCMGLResponse(const String& response);
      processCMGLResponse(manualCMGLBuffer);
      manualCMGLBuffer = "";
    } else if (line == "ERROR") {
      manualCMGLMode = false;
      LOGE("SMS_MANUAL", "sms_cmgl_manual_fail");
      manualCMGLBuffer = "";
    }
    return;
  }
  
  // 处理短信列表查询响应（只在非CMGL等待模式下）
  if (line.startsWith("+CMGL:") && !waitingForSMSRead && !manualCMGLMode) {
    LOGI("SMS_LIST", "sms_list_found", line.c_str());
    
    // 解析索引并读取短信
    int commaPos = line.indexOf(',');
    if (commaPos > 0) {
      String indexStr = line.substring(7, commaPos);
      indexStr.trim();
      int smsIndex = indexStr.toInt();
      if (smsIndex > 0) {
        LOGI("SMS_PROCESS", "sms_process_index", String(smsIndex).c_str());
        readSMSByIndex(smsIndex);
      }
    }
    return;
  }
  
  // 处理新短信通知
  if (line.startsWith("+CMTI")) {
    LOGI("SMS_NOTIFY", "sms_notify", line.c_str());
    
    if (simState != SIM_STATE_READY) {
      LOGW("SMS", "sms_sim_not_ready_skip");
      return;
    }
    
    // 解析短信索引
    int commaPos = line.lastIndexOf(',');
    if (commaPos <= 0) return;
    
    String indexStr = line.substring(commaPos + 1);
    indexStr.trim();
    int smsIndex = indexStr.toInt();
    if (smsIndex <= 0) return;
    
    bool queueWasEmpty = pendingSMSCount == 0;
    bool exists = false;
    for (int i = 0; i < pendingSMSCount; i++) {
      if (pendingSMSIndexes[i] == smsIndex) {
        exists = true;
        break;
      }
    }
    if (!exists) {
      if (pendingSMSCount < MAX_PENDING_SMS_INDEXES) {
        pendingSMSIndexes[pendingSMSCount++] = smsIndex;
        LOGI("SMS", queueWasEmpty ? "sms_first_notify_wait" : "sms_pending_add", String(smsIndex).c_str());
      } else {
        requestPendingSMSFullScan(smsIndex);
      }
    }

    // Later notifications join the existing bounded batch or active read queue.
    if (queueWasEmpty && !waitingForSMSRead && !waitingForSMSDeleteResponse) {
      pendingSMSProcessing = true;
      firstSMSTime = millis();
    }
    
    return;
  }
  
  // 处理CMT PDU第二行（纯hex数据）
  if (awaitingCmtPdu) {
    String pduLine = line;
    pduLine.trim();

    if (!isLikelyPduPayloadLine(pduLine)) {
      LOGW("CMT_PARSE", "cmt_pdu_length_fail", String(expectedPDULenChars / 2).c_str(), String(pduLine.length()).c_str());
      if (line == "OK" || line == "ERROR" || line.startsWith("+CME ERROR") || line.startsWith("+CMS ERROR")) {
        expectedPDULenChars = 0;
        awaitingCmtPdu = false;
        return;
      }
    } else {
      awaitingCmtPdu = false; // 取到PDU第二行才清掉等待状态
    
      // 使用新的PDU长度验证函数
      extern bool validatePduLength(const String &pduHex, int tpduLength);
      int tpduLength = expectedPDULenChars / 2; // 转换为字节数
      if (expectedPDULenChars > 0 && !validatePduLength(pduLine, tpduLength)) {
        LOGW("CMT_PARSE", "cmt_pdu_length_fail", String(tpduLength).c_str(), String(pduLine.length()).c_str());
        // 仍然处理数据，但记录警告
      }
    
      // 存进待处理队列（只存纯HEX，不带+CMT行）
      extern void storePendingCMTSMS(const String &pduHex);
      storePendingCMTSMS(pduLine);
      LOGD("SMS_CMT", "sms_cmt_store_pending", String(pendingSMSCount + 1).c_str());
      return;
    }
  }
  
  // 处理直接短信内容通知
  if (line.startsWith("+CMT:")) {
    LOGI("SMS_CMT", "sms_cmt_direct", line.c_str());
    
    if (simState != SIM_STATE_READY) {
      LOGW("SMS", "sms_sim_not_ready_skip");
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
      LOGD("CMT_PARSE", "cmt_length_parse", String(lengthBytes).c_str());
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
          LOGD("CMT_PARSE", "cmt_length_parse_simple", String(lengthBytes).c_str());
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
      LOGI("SMS", "sms_cmt_first_wait");
      pendingSMSProcessing = true;
      firstSMSTime = millis();
      pendingSMSCount = 0;
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
      LOGD("INIT_OK", "init_cmd_ok", initCmds[initCmdIndex]);
      initCmdIndex++;
      if (config.debug.atCommandEcho) {
        Serial.println("[INIT_CMD] Completed cmd " + String(initCmdIndex-1) + ", next: " + String(initCmdIndex));
      }
      if (initCmdIndex >= INIT_CMD_COUNT) {
        LOGI("SIM7670G", "sim_init_done");
        changeState(SIM_STATE_CONFIG_APN);
        sendNetworkConfig();
      }
    } else if (simState == SIM_STATE_CONFIG_APN) {
      if (!sendNextNetworkConfigCommand()) {
        LOGI("SIM7670G", "sim_network_ready");
        changeState(SIM_STATE_READY);
      }
    }
    return;
  }
  
  // 处理ERROR响应
  if (line == "ERROR" || line.startsWith("+CME ERROR") || line.startsWith("+CMS ERROR")) {
    if (line.indexOf("Last PDN disconnection not allowed") >= 0 &&
        config.network.dataPolicy == DATA_POLICY_ALWAYS_OFF) {
      LOGW("NET_CFG", "net_cfg_ignore_cgact", line.c_str());
      waitingForResponse = false;
      cmdRetryCount = 0;
      if (simState == SIM_STATE_CONFIG_APN) {
        if (!sendNextNetworkConfigCommand()) {
          changeState(SIM_STATE_READY);
        }
      }
      return;
    }

    waitingForResponse = false;
    if (simState != SIM_STATE_CONFIG_APN) {
      LOGW("AT_ERROR", "at_error", line.c_str());
    }
    
    if (simState == SIM_STATE_WAIT_AT_OK) {
      // AT测试失败，增加重试计数
      atRetryCount++;
      LOGD("AT_ERROR", "at_test_retry", String(atRetryCount).c_str());
    } else if (simState == SIM_STATE_INIT_CMDS) {
      // 初始化指令失败，重试当前指令
      LOGW("INIT_ERROR", "init_cmd_fail", initCmds[initCmdIndex], line.c_str());
      cmdRetryCount++;
      if (cmdRetryCount > 3) {
        LOGE("INIT", "init_cmd_skip", initCmds[initCmdIndex]);
        initCmdIndex++;
        cmdRetryCount = 0;
      }
    } else if (simState == SIM_STATE_CONFIG_APN) {
      String command = currentNetworkConfigCommand();
      if (command.startsWith("AT+CNMP=")) {
        LOGW("NET_CFG", "net_cfg_cmd_skip", command.c_str(), line.c_str());
        cmdRetryCount = 0;
        if (!sendNextNetworkConfigCommand()) {
          changeState(SIM_STATE_READY);
        }
        return;
      }
      cmdRetryCount++;
      if (cmdRetryCount > 3) {
        LOGW("NET_CFG", "net_cfg_cmd_skip", command.c_str(), line.c_str());
        cmdRetryCount = 0;
        if (!sendNextNetworkConfigCommand()) {
          changeState(SIM_STATE_READY);
        }
      } else {
        LOGW("NET_CFG", "net_cfg_cmd_retry", command.c_str(), line.c_str());
        if (!resendCurrentNetworkConfigCommand()) {
          changeState(SIM_STATE_READY);
        }
      }
    }
    return;
  }
  
  if (line.indexOf("READY") >= 0 && simState == SIM_STATE_INIT_CMDS) {
    LOGI("SIM", "sim_ready");
  }
  
  // 状态解析已移动到系统状态管理器
  // 只保留IP地址和PING响应的日志
  if (line.indexOf("+CGPADDR:") >= 0) {
    LOGI("IP", "ip_address", line.c_str());
  }
  
  if (line.indexOf("+CPING:") >= 0) {
    if (line.indexOf(",3,") >= 0) {
      LOGI("PING", "ping_done");
    } else {
      LOGD("PING", "ping_line", line.c_str());
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
      
      LOGI("SMS_MANUAL", "sms_manual_count", String(totalSMSCount).c_str(), String(maxSMSIndex).c_str());
      
      if (totalSMSCount > 0) {
        // 开始手动CMGR轮询
        manualCMGRMode = true;
        currentCMGRIndex = 1;
        foundSMSCount = 0;
        extern void clearTempSMSStorage();
        clearTempSMSStorage();
        
        LOGI("SMS_MANUAL", "sms_manual_cmgr_start");
        // 读取第一条短信
        readSMSByIndex(currentCMGRIndex);
      } else {
        LOGI("SMS_MANUAL", "sms_manual_none");
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

  rxBuffer.reserve(512);
  smsReadBuffer.reserve(1024);
  manualCMGLBuffer.reserve(1024);
  
  sim7670g.begin(115200, SERIAL_8N1, SIM7670G_RX_PIN, SIM7670G_TX_PIN);
  sim7670g.setTimeout(120);
  
  while (sim7670g.available()) {
    sim7670g.read();
  }
  
  if (config.debug.atCommandEcho) {
    Serial.println("[INIT] 串口初始化完成: RX=" + String(SIM7670G_RX_PIN) + ", TX=" + String(SIM7670G_TX_PIN));
  }
  
  LOGI("SIM7670G", "sim_power_on_start");
  powerPulsePwrKey();
  changeState(SIM_STATE_WAIT_BOOT);
}

void simTask() {
  switch (simState) {
    case SIM_STATE_IDLE:
      // 空闲状态，等待初始化
      break;
      
    case SIM_STATE_WAIT_BOOT:
      if (millisElapsed(millis(), stateStartMs, 20000UL)) {  // 20秒启动时间
        LOGI("SIM7670G", "sim_at_test_start");
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
      if ((!waitingForResponse && atRetryCount > 0) ||
          (waitingForResponse && millisElapsed(millis(), cmdSendMs, 3000UL))) {
        atRetryCount++;
        if (atRetryCount > 15) {
          LOGE("SIM7670G", "sim_at_test_fail_restart");
          atRetryCount = 0;
          changeState(SIM_STATE_IDLE);
          delay(2000);
          initSIM7670G();
          return;
        }
        LOGD("AT_RETRY", "sim_at_retry", String(atRetryCount).c_str());
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
        } else if (millisElapsed(millis(), cmdSendMs, 3000UL)) {
          // 指令超时
          cmdRetryCount++;
          if (cmdRetryCount > 3) {
            LOGE("INIT", "init_cmd_timeout_skip", initCmds[initCmdIndex]);
            initCmdIndex++;
            cmdRetryCount = 0;
            waitingForResponse = false;
          } else {
            LOGW("INIT", "init_cmd_timeout_retry", initCmds[initCmdIndex]);
            sendAT(initCmds[initCmdIndex]);
          }
        }
      } else {
        // 所有初始化指令完成
        LOGI("SIM7670G", "sim_init_done");
        changeState(SIM_STATE_CONFIG_APN);
        sendNetworkConfig();
      }
      break;
      
    case SIM_STATE_CONFIG_APN:
      if (waitingForResponse && millisElapsed(millis(), cmdSendMs, 5000UL)) {
        String command = currentNetworkConfigCommand();
        cmdRetryCount++;
        if (cmdRetryCount > 3) {
          LOGW("NET_CFG", "net_cfg_cmd_timeout_skip", command.c_str());
          waitingForResponse = false;
          cmdRetryCount = 0;
          if (!sendNextNetworkConfigCommand()) {
            changeState(SIM_STATE_READY);
          }
        } else {
          LOGW("NET_CFG", "net_cfg_cmd_timeout_retry", command.c_str());
          waitingForResponse = false;
          if (!resendCurrentNetworkConfigCommand()) {
            changeState(SIM_STATE_READY);
          }
        }
      }
      break;
      
    case SIM_STATE_READY:
      // 模块就绪，更新系统状态
      {
        static unsigned long lastStatusUpdate = 0;
        static unsigned long lastSMSCheck = 0;
        uint32_t now = millis();
        
        if (millisElapsed(now, lastStatusUpdate, 30000UL)) { // 30秒更新一次
          systemStatus.updateStatus();
          lastStatusUpdate = now;
        }
        
        // 定期检查短信通知配置
        if (millisElapsed(now, lastSMSCheck, 30000UL)) { // 30秒检查一次
          checkSMSNotificationConfig();
          lastSMSCheck = now;
        }
        
        // 检查CMGL超时
        if (waitingForSMSRead && currentSMSIndex == -1 && cmglReceiving && 
            millisElapsed(now, cmglStartTime, CMGL_TIMEOUT)) {
          LOGI("SMS_CMGL", "sms_cmgl_timeout_process");
          waitingForSMSRead = false;
          cmglReceiving = false;
          extern void processCMGLResponse(const String& response);
          processCMGLResponse(smsReadBuffer);
          smsReadBuffer = "";
        }
        
        // 检查手动CMGL超时
        if (manualCMGLMode && manualCMGLReceiving && 
            millisElapsed(now, manualCMGLStartTime, CMGL_TIMEOUT)) {
          LOGI("SMS_MANUAL", "sms_cmgl_manual_timeout");
          manualCMGLMode = false;
          extern void processCMGLResponse(const String& response);
          processCMGLResponse(manualCMGLBuffer);
          manualCMGLBuffer = "";
        }
        
        // 检查是否需要处理待处理的短信
        if (pendingSMSProcessing && millisElapsed(now, firstSMSTime, SMS_MERGE_DELAY)) {
          LOGI("SMS", "sms_merge_timeout_process");
          pendingSMSProcessing = false;
          
          // 处理CMT短信
          extern void processPendingCMTSMS();
          processPendingCMTSMS();
        }

        if (!pendingSMSProcessing && !isModemBusyForStatus()) {
          if (sendNextSMSDelete()) {
            break;
          }
          if (readNextPendingSMS()) {
            break;
          }
          if (pendingSMSFullScanRequested || initialSMSScanRequested) {
            pendingSMSFullScanRequested = false;
            initialSMSScanRequested = false;
            LOGW("SMS", "sms_pending_full_scan");
            checkAllSMS();
          }
        }
      }
      break;
      
    default:
      LOGE("STATE", "sim_state_unknown", String(simState).c_str());
      changeState(SIM_STATE_IDLE);
      break;
  }
  
  // 定期输出状态信息用于调试
  static unsigned long lastDebugOutput = 0;
  uint32_t debugNow = millis();
  if (millisElapsed(debugNow, lastDebugOutput, 10000UL)) { // 每10秒输出一次
    String stateNames[] = {"IDLE", "POWER_ON", "WAIT_BOOT", "WAIT_AT_OK", "INIT_CMDS", "CONFIG_APN", "READY"};
    LOGI("SIM_STATUS", "sim_status",
         stateNames[simState].c_str(),
         (simState == SIM_STATE_READY) ? i18nGet("bool_yes") : i18nGet("bool_no"));
    lastDebugOutput = debugNow;
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
  LOGI("WEB_AT", "web_at_send", command.c_str());
  
  sim7670g.println(command);
  sim7670g.flush();
  
  // 简单等待响应
  String response = "";
  unsigned long startTime = millis();
  while (!millisElapsed(millis(), startTime, 3000UL)) {
    if (sim7670g.available()) {
      response += sim7670g.readString();
      break;
    }
    delay(10);
  }
  
  LOGI("WEB_AT", "web_at_response", response.c_str());
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
  LOGI("SIM", "sim_reset_check");
}

// 检查短信通知配置
void checkSMSNotificationConfig() {
  if (simState != SIM_STATE_READY) return;
  if (isModemBusyForStatus()) return;
  
  LOGD("SMS_CFG", "sms_cfg_check");
  sendAT("AT+CNMI?");
}

// 手动查询所有短信
void checkAllSMS() {
  if (simState != SIM_STATE_READY || waitingForSMSRead || manualCMGLMode || manualCMGRMode) return;
  
  LOGI("SMS_MANUAL", "sms_manual_check_start");
  
  // 使用CPMS查询短信数量
  sim7670g.println("AT+CPMS?");
  sim7670g.flush();
}

void requestSMSFullScan() {
  pendingSMSFullScanRequested = true;
}

ModemAsyncSubmitStatus queueAsyncATCommand(const String& command, uint32_t& jobIdOut) {
  jobIdOut = 0;
  if (!lockModemAsyncJobs()) return MODEM_ASYNC_SUBMIT_UNAVAILABLE;

  bool busy = modemAsyncWorkerActive ||
              (asyncATJob.active && !asyncATJob.complete) ||
              (asyncSMSJob.active && !asyncSMSJob.complete);
  if (busy) {
    unlockModemAsyncJobs();
    return MODEM_ASYNC_SUBMIT_BUSY;
  }

  asyncATJob.active = true;
  asyncATJob.running = false;
  asyncATJob.complete = false;
  asyncATJob.id = allocateModemAsyncJobId();
  asyncATJob.command = command;
  asyncATJob.response = "";
  jobIdOut = asyncATJob.id;
  unlockModemAsyncJobs();
  return MODEM_ASYNC_SUBMIT_ACCEPTED;
}

ModemAsyncSubmitStatus queueAsyncSMS(const String& phoneNumber, const String& message, uint32_t& jobIdOut) {
  jobIdOut = 0;
  if (!lockModemAsyncJobs()) return MODEM_ASYNC_SUBMIT_UNAVAILABLE;

  bool busy = modemAsyncWorkerActive ||
              (asyncATJob.active && !asyncATJob.complete) ||
              (asyncSMSJob.active && !asyncSMSJob.complete);
  if (busy) {
    unlockModemAsyncJobs();
    return MODEM_ASYNC_SUBMIT_BUSY;
  }

  asyncSMSJob.active = true;
  asyncSMSJob.running = false;
  asyncSMSJob.complete = false;
  asyncSMSJob.success = false;
  asyncSMSJob.id = allocateModemAsyncJobId();
  asyncSMSJob.phoneNumber = phoneNumber;
  asyncSMSJob.message = message;
  asyncSMSJob.error = "";
  jobIdOut = asyncSMSJob.id;
  unlockModemAsyncJobs();
  return MODEM_ASYNC_SUBMIT_ACCEPTED;
}

bool getAsyncATJobResult(uint32_t jobId, ModemAsyncJobResult& resultOut) {
  if (!lockModemAsyncJobs()) return false;
  uint32_t requestedId = jobId == 0 ? asyncATJob.id : jobId;
  if (!asyncATJob.active || asyncATJob.id != requestedId) {
    unlockModemAsyncJobs();
    return false;
  }

  resultOut.id = asyncATJob.id;
  resultOut.queued = !asyncATJob.running && !asyncATJob.complete;
  resultOut.running = asyncATJob.running;
  resultOut.complete = asyncATJob.complete;
  resultOut.success = asyncATJob.complete && !asyncATJob.response.startsWith("ERROR:");
  resultOut.response = asyncATJob.response;
  resultOut.error = "";
  unlockModemAsyncJobs();
  return true;
}

bool getAsyncSMSJobResult(uint32_t jobId, ModemAsyncJobResult& resultOut) {
  if (!lockModemAsyncJobs()) return false;
  uint32_t requestedId = jobId == 0 ? asyncSMSJob.id : jobId;
  if (!asyncSMSJob.active || asyncSMSJob.id != requestedId) {
    unlockModemAsyncJobs();
    return false;
  }

  resultOut.id = asyncSMSJob.id;
  resultOut.queued = !asyncSMSJob.running && !asyncSMSJob.complete;
  resultOut.running = asyncSMSJob.running;
  resultOut.complete = asyncSMSJob.complete;
  resultOut.success = asyncSMSJob.success;
  resultOut.response = "";
  resultOut.error = asyncSMSJob.error;
  unlockModemAsyncJobs();
  return true;
}

bool hasActiveModemAsyncJob() {
  if (!lockModemAsyncJobs()) return true;
  bool active = modemAsyncWorkerActive ||
                (asyncATJob.active && !asyncATJob.complete) ||
                (asyncSMSJob.active && !asyncSMSJob.complete);
  unlockModemAsyncJobs();
  return active;
}

bool isModemAsyncJobRunning() {
  return modemAsyncWorkerActive;
}

static void modemAsyncWorkerTask(void* parameter) {
  (void)parameter;
  watchdogManager.registerTask(xTaskGetCurrentTaskHandle());
  ModemAsyncJobType jobType = modemAsyncWorkerJobType;

  if (jobType == MODEM_ASYNC_JOB_AT) {
    String command;
    uint32_t jobId = 0;
    if (lockModemAsyncJobs(portMAX_DELAY)) {
      command = asyncATJob.command;
      jobId = asyncATJob.id;
      unlockModemAsyncJobs();
    }
    String response = sendATCommand(command);
    if (lockModemAsyncJobs(portMAX_DELAY)) {
      if (asyncATJob.id == jobId) {
        asyncATJob.response = response;
        asyncATJob.complete = true;
        asyncATJob.running = false;
      }
      unlockModemAsyncJobs();
    }
  } else if (jobType == MODEM_ASYNC_JOB_SMS) {
    String phoneNumber;
    String message;
    uint32_t jobId = 0;
    if (lockModemAsyncJobs(portMAX_DELAY)) {
      phoneNumber = asyncSMSJob.phoneNumber;
      message = asyncSMSJob.message;
      jobId = asyncSMSJob.id;
      unlockModemAsyncJobs();
    }
    bool success = simState == SIM_STATE_READY && sendSMS(phoneNumber, message);
    if (lockModemAsyncJobs(portMAX_DELAY)) {
      if (asyncSMSJob.id == jobId) {
        asyncSMSJob.success = success;
        asyncSMSJob.error = success ? "" : i18nGet(simState == SIM_STATE_READY ? "web_sms_send_fail" : "web_sim_not_ready");
        asyncSMSJob.complete = true;
        asyncSMSJob.running = false;
      }
      unlockModemAsyncJobs();
    }
  }

  if (lockModemAsyncJobs(portMAX_DELAY)) {
    modemAsyncRecoveryScanPending = true;
    modemAsyncWorkerJobType = MODEM_ASYNC_JOB_NONE;
    modemAsyncWorkerActive = false;
    modemAsyncWorkerTaskHandle = nullptr;
    unlockModemAsyncJobs();
  }
  watchdogManager.unregisterTask(xTaskGetCurrentTaskHandle());
  vTaskDelete(nullptr);
}

void processModemAsyncJobs() {
  if (!lockModemAsyncJobs(0)) return;
  if (modemAsyncWorkerActive) {
    unlockModemAsyncJobs();
    return;
  }
  if (modemAsyncRecoveryScanPending) {
    modemAsyncRecoveryScanPending = false;
    unlockModemAsyncJobs();
    requestSMSFullScan();
    return;
  }

  ModemAsyncJobType jobType = MODEM_ASYNC_JOB_NONE;
  if (asyncATJob.active && !asyncATJob.running && !asyncATJob.complete) {
    asyncATJob.running = true;
    jobType = MODEM_ASYNC_JOB_AT;
  } else if (asyncSMSJob.active && !asyncSMSJob.running && !asyncSMSJob.complete) {
    asyncSMSJob.running = true;
    jobType = MODEM_ASYNC_JOB_SMS;
  }

  if (jobType == MODEM_ASYNC_JOB_NONE) {
    unlockModemAsyncJobs();
    return;
  }

  modemAsyncWorkerJobType = jobType;
  modemAsyncWorkerActive = true;
  unlockModemAsyncJobs();

  BaseType_t created = xTaskCreatePinnedToCore(
    modemAsyncWorkerTask,
    "sim_async",
    12288,
    nullptr,
    1,
    &modemAsyncWorkerTaskHandle,
    1
  );
  if (created != pdPASS) {
    if (lockModemAsyncJobs(portMAX_DELAY)) {
      if (jobType == MODEM_ASYNC_JOB_AT) {
        asyncATJob.response = "ERROR: worker_start_failed";
        asyncATJob.complete = true;
        asyncATJob.running = false;
      } else {
        asyncSMSJob.success = false;
        asyncSMSJob.error = "worker_start_failed";
        asyncSMSJob.complete = true;
        asyncSMSJob.running = false;
      }
      unlockModemAsyncJobs();
    }
    modemAsyncWorkerJobType = MODEM_ASYNC_JOB_NONE;
    modemAsyncWorkerActive = false;
    modemAsyncWorkerTaskHandle = nullptr;
  }
}

// 网络配置指令
void sendNetworkConfig() {
  resetNetworkConfigQueue();

  // 设置运营商选择
  String operatorCmd;
  String defaultApn = "CMNET";
  int dataPolicy = config.network.dataPolicy;
  bool enableData = true;
  int radioMode = config.network.radioMode;

  if (radioMode > 0) {
    String modeCmd = "AT+CNMP=" + String(radioMode);
    LOGI("NET_CFG", "net_cfg_set_radio", modeCmd.c_str());
    enqueueNetworkConfigCommand(modeCmd);
  }
  switch (config.network.operatorMode) {
    case 0: // 自动选网
      operatorCmd = "AT+COPS=0";
      LOGI("NET_CFG", "net_cfg_operator_auto");
      break;
    case 1: // 中国移动
      operatorCmd = "AT+COPS=1,2,\"46000\",7";
      LOGI("NET_CFG", "net_cfg_operator_cmcc");
      break;
    case 2: // 中国联通
      operatorCmd = "AT+COPS=1,2,\"46001\",7";
      LOGI("NET_CFG", "net_cfg_operator_cu");
      defaultApn = "3gnet";
      break;
    case 3: // 中国电信
      operatorCmd = "AT+COPS=1,2,\"46003\",7";
      LOGI("NET_CFG", "net_cfg_operator_ct");
      defaultApn = "ctnet";
      break;
    case 4: // 英国 giffgaff (O2)
      operatorCmd = "AT+COPS=0";
      LOGI("NET_CFG", "net_cfg_operator_giffgaff");
      defaultApn = "giffgaff.com";
      break;
    default:
      operatorCmd = "AT+COPS=0";
      break;
  }
  
  enqueueNetworkConfigCommand(operatorCmd);
  
  String apn = config.network.apn.isEmpty() ? defaultApn : config.network.apn;
  LOGI("NET_CFG", "net_cfg_apply", operatorCmd.c_str(), apn.c_str());

  if (dataPolicy == DATA_POLICY_ALWAYS_OFF) {
    enableData = false;
  } else {
    enableData = true;
  }

  if (enableData) {
    String apnCmd = "AT+CGDCONT=1,\"IP\",\"" + apn + "\"";
    enqueueNetworkConfigCommand(apnCmd);
    
    // 3. 激活PDP上下文
    LOGI("NET_CFG", "net_cfg_pdp_activate");
    enqueueNetworkConfigCommand("AT+CGACT=1,1");
    
    // 4. 获取IP地址
    enqueueNetworkConfigCommand("AT+CGPADDR=1");
  } else {
    LOGI("NET_CFG", "net_cfg_data_disabled");
  }

  cmdRetryCount = 0;
  waitingForResponse = false;
  if (!sendNextNetworkConfigCommand()) {
    LOGI("SIM7670G", "sim_network_ready");
    changeState(SIM_STATE_READY);
  }
}

// 网络测试已移动到系统状态管理器

static bool readNextPendingSMS() {
  if (pendingSMSCount <= 0 || isModemBusyForStatus()) return false;

  int smsIndex = pendingSMSIndexes[0];
  for (int index = 1; index < pendingSMSCount; index++) {
    pendingSMSIndexes[index - 1] = pendingSMSIndexes[index];
  }
  pendingSMSCount--;
  readSMSByIndex(smsIndex);
  return true;
}

void queueSMSDelete(int index) {
  if (index <= 0 || index == currentSMSDeleteIndex) return;

  for (int queueIndex = 0; queueIndex < pendingSMSDeleteCount; queueIndex++) {
    if (pendingSMSDeleteIndexes[queueIndex] == index) return;
  }
  if (pendingSMSDeleteCount >= MAX_PENDING_SMS_DELETES) {
    logManager.addLog(LOG_ERROR, "SMS_DEL", "Delete queue full for index " + String(index));
    return;
  }
  pendingSMSDeleteIndexes[pendingSMSDeleteCount++] = index;
}

static bool sendNextSMSDelete() {
  if (pendingSMSDeleteCount <= 0 || isModemBusyForStatus()) return false;

  currentSMSDeleteIndex = pendingSMSDeleteIndexes[0];
  for (int index = 1; index < pendingSMSDeleteCount; index++) {
    pendingSMSDeleteIndexes[index - 1] = pendingSMSDeleteIndexes[index];
  }
  pendingSMSDeleteCount--;

  String command = "AT+CMGD=" + String(currentSMSDeleteIndex);
  LOGD("SMS_DEL", "sms_delete", String(currentSMSDeleteIndex).c_str());
  if (config.debug.atCommandEcho) {
    logManager.addLog(LOG_DEBUG, "AT_TX", command);
  }
  sim7670g.println(command);
  sim7670g.flush();
  waitingForSMSDeleteResponse = true;
  return true;
}

// 读取指定索引的短信
void readSMSByIndex(int index) {
  LOGI("SMS_READ", "sms_read_start", String(index).c_str());
  
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

static String hexByte(uint8_t value) {
  char buffer[3];
  snprintf(buffer, sizeof(buffer), "%02X", value);
  return String(buffer);
}

static void appendUtf16CodeUnitHex(String& outputHex, uint16_t codeUnit) {
  outputHex += hexByte((codeUnit >> 8) & 0xFF);
  outputHex += hexByte(codeUnit & 0xFF);
}

static bool appendUtf16BeHexFromUtf8(const String& text, String& outputHex) {
  outputHex = "";
  int charIndex = 0;
  while (charIndex < text.length()) {
    uint8_t firstByte = static_cast<uint8_t>(text.charAt(charIndex));
    uint32_t codePoint = 0;
    int expectedContinuationBytes = 0;

    if (firstByte < 0x80) {
      codePoint = firstByte;
    } else if ((firstByte & 0xE0) == 0xC0) {
      codePoint = firstByte & 0x1F;
      expectedContinuationBytes = 1;
    } else if ((firstByte & 0xF0) == 0xE0) {
      codePoint = firstByte & 0x0F;
      expectedContinuationBytes = 2;
    } else if ((firstByte & 0xF8) == 0xF0) {
      codePoint = firstByte & 0x07;
      expectedContinuationBytes = 3;
    } else {
      codePoint = 0xFFFD;
    }

    if (expectedContinuationBytes > 0) {
      if (charIndex + expectedContinuationBytes >= text.length()) return false;
      for (int continuationIndex = 1; continuationIndex <= expectedContinuationBytes; continuationIndex++) {
        uint8_t nextByte = static_cast<uint8_t>(text.charAt(charIndex + continuationIndex));
        if ((nextByte & 0xC0) != 0x80) return false;
        codePoint = (codePoint << 6) | (nextByte & 0x3F);
      }
    }

    if (codePoint <= 0xFFFF) {
      appendUtf16CodeUnitHex(outputHex, static_cast<uint16_t>(codePoint));
    } else if (codePoint <= 0x10FFFF) {
      uint32_t adjusted = codePoint - 0x10000;
      uint16_t highSurrogate = 0xD800 | ((adjusted >> 10) & 0x3FF);
      uint16_t lowSurrogate = 0xDC00 | (adjusted & 0x3FF);
      appendUtf16CodeUnitHex(outputHex, highSurrogate);
      appendUtf16CodeUnitHex(outputHex, lowSurrogate);
    } else {
      appendUtf16CodeUnitHex(outputHex, 0xFFFD);
    }

    charIndex += expectedContinuationBytes + 1;
  }
  return true;
}

static String encodeSmsAddressSemiOctets(const String& phoneNumber, String& digitsOut, bool& internationalOut) {
  digitsOut = "";
  internationalOut = phoneNumber.startsWith("+");
  for (int charIndex = 0; charIndex < phoneNumber.length(); charIndex++) {
    char value = phoneNumber.charAt(charIndex);
    if (value >= '0' && value <= '9') {
      digitsOut += value;
    }
  }

  String paddedDigits = digitsOut;
  if ((paddedDigits.length() % 2) != 0) {
    paddedDigits += "F";
  }

  String encoded = "";
  for (int digitIndex = 0; digitIndex < paddedDigits.length(); digitIndex += 2) {
    encoded += paddedDigits.charAt(digitIndex + 1);
    encoded += paddedDigits.charAt(digitIndex);
  }
  return encoded;
}

static bool buildSmsSubmitPdu(const String& phoneNumber, const String& message, String& pduOut, int& tpduLengthOut) {
  String userDataHex;
  if (!appendUtf16BeHexFromUtf8(message, userDataHex)) return false;
  int userDataBytes = userDataHex.length() / 2;
  if (userDataBytes <= 0 || userDataBytes > 140) return false;

  String digits;
  bool international = false;
  String encodedAddress = encodeSmsAddressSemiOctets(phoneNumber, digits, international);
  if (digits.isEmpty()) return false;

  pduOut = "00";                         // SMSC: use modem default
  pduOut += "01";                        // SMS-SUBMIT, no validity period
  pduOut += "00";                        // TP-MR
  pduOut += hexByte(digits.length());     // TP-DA length in digits
  pduOut += international ? "91" : "81"; // TP-DA type
  pduOut += encodedAddress;
  pduOut += "00";                        // TP-PID
  pduOut += "08";                        // TP-DCS UCS2
  pduOut += hexByte(userDataBytes);       // TP-UDL in octets for UCS2
  pduOut += userDataHex;

  tpduLengthOut = (pduOut.length() / 2) - 1;
  return tpduLengthOut > 0;
}

static int findModemErrorIndex(const String& response) {
  int idx = response.indexOf("+CMS ERROR");
  if (idx >= 0) return idx;
  idx = response.indexOf("+CME ERROR");
  if (idx >= 0) return idx;
  return response.indexOf("ERROR");
}

static bool responseHasCompleteErrorLine(const String& response) {
  int idx = findModemErrorIndex(response);
  if (idx < 0) return false;
  return response.indexOf('\r', idx) >= 0 || response.indexOf('\n', idx) >= 0;
}

static String compactAtResponse(String response) {
  response.replace('\r', ' ');
  response.replace('\n', ' ');
  while (response.indexOf("  ") >= 0) {
    response.replace("  ", " ");
  }
  response.trim();
  return response;
}

static bool collectSmsAtResponse(const char* command, unsigned long timeoutMs, String& responseOut) {
  while (sim7670g.available()) {
    sim7670g.read();
  }
  if (config.debug.atCommandEcho) {
    logManager.addLog(LOG_DEBUG, "AT_TX", command);
  }
  sim7670g.println(command);
  sim7670g.flush();

  responseOut = "";
  unsigned long startTime = millis();
  while (!millisElapsed(millis(), startTime, timeoutMs)) {
    while (sim7670g.available()) {
      responseOut += static_cast<char>(sim7670g.read());
      if (responseOut.indexOf("\r\nOK") >= 0 || responseOut.indexOf("\nOK") >= 0 || responseHasCompleteErrorLine(responseOut)) {
        return true;
      }
    }
    watchdogManager.feedWatchdog();
    delay(10);
  }
  return !responseOut.isEmpty();
}

static String regStatForLog(bool gotStat, int stat) {
  return gotStat ? String(stat) : String("unknown");
}

static bool isRegisteredStat(int stat) {
  return stat == 1 || stat == 5;
}

static bool querySmsRegistrationForSend(String& detailOut) {
  String ceregResp;
  String cregResp;
  int epsStat = -1;
  int csStat = -1;
  bool gotEps = false;
  bool gotCs = false;

  if (collectSmsAtResponse("AT+CEREG?", 3000, ceregResp)) {
    gotEps = parseRegStatFromResponse(ceregResp, "+CEREG:", epsStat);
  }
  if (collectSmsAtResponse("AT+CREG?", 3000, cregResp)) {
    gotCs = parseRegStatFromResponse(cregResp, "+CREG:", csStat);
  }

  bool epsRegistered = gotEps && isRegisteredStat(epsStat);
  bool csRegistered = gotCs && isRegisteredStat(csStat);
  bool gotLiveStatus = gotEps || gotCs;
  SystemStatus cachedStatus = systemStatus.getStatus();
  bool cachedRegistered = cachedStatus.csRegistered || cachedStatus.epsRegistered;
  detailOut = "CEREG=";
  detailOut += regStatForLog(gotEps, epsStat);
  detailOut += ", CREG=";
  detailOut += regStatForLog(gotCs, csStat);
  if (!gotLiveStatus) {
    detailOut += ", cache=";
    detailOut += cachedRegistered ? "registered" : "not_registered";
    detailOut += ", CEREG_RESP=";
    detailOut += compactAtResponse(ceregResp);
    detailOut += ", CREG_RESP=";
    detailOut += compactAtResponse(cregResp);
    return cachedRegistered;
  }
  return epsRegistered || csRegistered;
}

static bool waitForSmsExpected(const char* expected, unsigned long timeoutMs, String& responseOut) {
  responseOut = "";
  unsigned long startTime = millis();
  bool sawError = false;
  unsigned long errorSeenMs = 0;
  while (!millisElapsed(millis(), startTime, timeoutMs)) {
    while (sim7670g.available()) {
      responseOut += static_cast<char>(sim7670g.read());
      if (responseOut.indexOf(expected) >= 0) return true;
      if (findModemErrorIndex(responseOut) >= 0) {
        if (!sawError) {
          sawError = true;
          errorSeenMs = millis();
        }
      }
      if (sawError &&
          (responseHasCompleteErrorLine(responseOut) ||
           millisElapsed(millis(), errorSeenMs, 200UL))) {
        return false;
      }
    }
    watchdogManager.feedWatchdog();
    delay(10);
  }
  return false;
}

static bool waitForSmsResponse(const char* command, const char* expected, unsigned long timeoutMs, String& responseOut) {
  while (sim7670g.available()) {
    sim7670g.read();
  }
  if (config.debug.atCommandEcho) {
    logManager.addLog(LOG_DEBUG, "AT_TX", command);
  }
  sim7670g.println(command);
  sim7670g.flush();
  return waitForSmsExpected(expected, timeoutMs, responseOut);
}

static bool waitForSmsPrompt(unsigned long timeoutMs, String& responseOut) {
  return waitForSmsExpected(">", timeoutMs, responseOut);
}

// 发送短信（UCS2 PDU模式）
bool sendSMS(const String& phoneNumber, const String& message) {
  LOGI("SMS_SEND", "sms_send_to", phoneNumber.c_str());
  
  if (simState != SIM_STATE_READY) {
    LOGE("SMS_SEND", "sms_send_sim_not_ready");
    return false;
  }
  if (smsSending || isModemBusyForStatus()) {
    LOGW("SMS_SEND", "sms_send_busy");
    return false;
  }

  String registrationDetail;
  if (!querySmsRegistrationForSend(registrationDetail)) {
    LOGE("SMS_SEND", "sms_send_no_service_detail", registrationDetail.c_str());
    return false;
  }
  LOGD("SMS_SEND", "sms_send_registration", registrationDetail.c_str());
  smsSending = true;

  String pdu;
  int tpduLength = 0;
  if (!buildSmsSubmitPdu(phoneNumber, message, pdu, tpduLength)) {
    LOGE("SMS_SEND", "sms_send_fail", "invalid_or_too_long_message");
    smsSending = false;
    return false;
  }

  String response;
  if (!waitForSmsResponse("AT+CMGF=0", "OK", 3000, response)) {
    String responseLog = compactAtResponse(response);
    LOGE("SMS_SEND", "sms_send_fail", responseLog.c_str());
    smsSending = false;
    return false;
  }

  String cmgsCmd = "AT+CMGS=" + String(tpduLength);
  if (config.debug.atCommandEcho) {
    logManager.addLog(LOG_DEBUG, "AT_TX", cmgsCmd);
  }
  sim7670g.println(cmgsCmd);
  sim7670g.flush();

  if (!waitForSmsPrompt(5000, response)) {
    if (response.isEmpty()) {
      LOGE("SMS_SEND", "sms_send_prompt_timeout");
    } else {
      String responseLog = compactAtResponse(response);
      LOGE("SMS_SEND", "sms_send_cmgs_fail", responseLog.c_str());
    }
    smsSending = false;
    return false;
  }

  if (config.debug.atCommandEcho) {
    String pduLog = "[sms pdu len=";
    pduLog += String(pdu.length());
    pduLog += "]";
    logManager.addLog(LOG_DEBUG, "AT_TX", pduLog);
  }
  sim7670g.print(pdu);
  sim7670g.write(0x1A); // Ctrl+Z
  sim7670g.flush();
  
  // 等待发送结果
  bool success = waitForSmsExpected("+CMGS:", 30000, response);
  if (success) {
    LOGI("SMS_SEND", "sms_send_success");
  } else if (response.isEmpty()) {
    LOGE("SMS_SEND", "sms_send_timeout");
  } else {
    String responseLog = compactAtResponse(response);
    LOGE("SMS_SEND", "sms_send_fail", responseLog.c_str());
  }

  smsSending = false;
  return success;
}

// ========== 系统状态管理功能 ==========

void SystemStatusManager::initStatus() {
  LOGI("STATUS", "status_init_cache");
  
  status.signalStrength = -999;
  status.simReady = false;
  status.networkConnected = false;
  status.csRegistered = false;
  status.epsRegistered = false;
  status.dataAttached = false;
  status.operatorName = "Unknown";
  status.operatorCode = "Unknown";
  status.homeOperatorName = "Unknown";
  status.homeOperatorCode = "Unknown";
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
  
  if (isModemBusyForStatus()) {
    if (config.debug.atCommandEcho) {
      LOGD("STATUS", "status_busy_skip");
    }
    return;
  }

  if (needsUpdate()) {
    queryAllStatus();
    status.lastUpdate = millis();
  }
}

void SystemStatusManager::refreshSignalOnly() {
  if (simState == SIM_STATE_READY && !isModemBusyForStatus()) {
    querySignalStrength();
  }
}

void SystemStatusManager::refreshAllStatus() {
  if (simState == SIM_STATE_READY) {
    if (isModemBusyForStatus()) {
      return;
    }
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
  return millisElapsed(millis(), status.lastUpdate, interval);
}

void SystemStatusManager::queryAllStatus() {
  if (simState != SIM_STATE_READY) {
    if (config.debug.atCommandEcho) {
      LOGD("STATUS", "status_sim_not_ready_skip");
    }
    return;
  }
  if (isModemBusyForStatus()) {
    if (config.debug.atCommandEcho) {
      LOGD("STATUS", "status_busy_skip");
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
    LOGD("STATUS", "status_update",
         String(status.signalStrength).c_str(),
         status.operatorName.c_str(),
         status.networkType.c_str());
  }
}

void SystemStatusManager::querySignalStrength() {
  if (simState != SIM_STATE_READY) return;
  
  sim7670g.println("AT+CSQ");
  sim7670g.flush();
  
  // 等待响应并解析
  unsigned long startTime = millis();
  while (!millisElapsed(millis(), startTime, STATUS_QUERY_TIMEOUT_MS)) {
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
            if (!status.csRegistered && !status.epsRegistered) {
              status.signalStrength = -999;
            }
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
  if (status.simReady && (status.homeOperatorCode == "Unknown" || status.homeOperatorCode.isEmpty())) {
    sim7670g.println("AT+CIMI");
    sim7670g.flush();
    unsigned long startTime = millis();
    while (!millisElapsed(millis(), startTime, STATUS_QUERY_TIMEOUT_MS)) {
      if (sim7670g.available()) {
        String response = sim7670g.readString();
        String digits = extractImsiFromResponse(response);
        String code = extractHomeOperatorCodeFromImsi(digits);
        if (!code.isEmpty() && code != "Unknown") {
          status.homeOperatorCode = code;
          status.homeOperatorName = mapOperatorName(code);
        }
        break;
      }
      delay(10);
    }
  }
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
  while (!millisElapsed(millis(), startTime, STATUS_QUERY_TIMEOUT_MS)) {
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
  while (!millisElapsed(millis(), startTime, STATUS_QUERY_TIMEOUT_MS)) {
    if (sim7670g.available()) {
      String response = sim7670g.readString();
      if (parseRegStatFromResponse(response, "+CREG:", csStat)) {
        gotCs = true;
      }
      break;
    }
    delay(10);
  }

  bool updated = false;
  if (gotEps) {
    status.epsRegistered = (epsStat == 1 || epsStat == 5);
    updated = true;
  }
  if (gotCs) {
    status.csRegistered = (csStat == 1 || csStat == 5);
    updated = true;
  }
  if (updated) {
    status.networkConnected = status.epsRegistered || status.csRegistered;
    if (gotEps) {
      status.isRoaming = (epsStat == 5);
    } else if (gotCs) {
      status.isRoaming = (csStat == 5);
    }
  } else if (config.debug.atCommandEcho) {
    LOGD("STATUS", "status_no_reg_keep");
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
  while (!millisElapsed(millis(), startTime, STATUS_QUERY_TIMEOUT_MS)) {
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
    status.operatorCode = "Unknown";
    status.networkType = "Unknown";
    return;
  }
  
  sim7670g.println("AT+COPS?");
  sim7670g.flush();
  
  unsigned long startTime = millis();
  while (!millisElapsed(millis(), startTime, STATUS_QUERY_TIMEOUT_MS)) {
    if (sim7670g.available()) {
      String response = sim7670g.readString();
      if (response.indexOf("+COPS:") >= 0) {
        int lineStart = response.indexOf("+COPS:");
        int lineEnd = response.indexOf('\n', lineStart);
        String line = (lineStart >= 0) ? response.substring(lineStart, lineEnd >= 0 ? lineEnd : response.length()) : response;
        int colon = line.indexOf(':');
        String fields = (colon >= 0) ? line.substring(colon + 1) : line;
        fields.trim();
        int firstComma = fields.indexOf(',');
        int secondComma = (firstComma >= 0) ? fields.indexOf(',', firstComma + 1) : -1;
        int thirdComma = (secondComma >= 0) ? fields.indexOf(',', secondComma + 1) : -1;
        String formatStr = (secondComma > firstComma && firstComma >= 0) ? fields.substring(firstComma + 1, secondComma) : "";
        formatStr.trim();
        int format = formatStr.toInt();
        String oper = (secondComma >= 0)
          ? fields.substring(secondComma + 1, thirdComma >= 0 ? thirdComma : fields.length())
          : "";
        oper.trim();
        if (oper.startsWith("\"") && oper.endsWith("\"") && oper.length() >= 2) {
          oper = oper.substring(1, oper.length() - 1);
        }
        if (!oper.isEmpty()) {
          status.operatorName = mapOperatorName(oper);
          bool numeric = true;
          for (int i = 0; i < oper.length(); i++) {
            char c = oper.charAt(i);
            if (c < '0' || c > '9') {
              numeric = false;
              break;
            }
          }
          if (format == 2 && numeric) {
            status.operatorCode = oper;
            status.operatorName = mapOperatorName(oper);
          } else if (numeric) {
            status.operatorCode = oper;
          }
        }
        
        // 解析网络类型(仅在获取到AcT字段时更新)
        if (thirdComma >= 0) {
          String actStr = fields.substring(thirdComma + 1);
          actStr.trim();
          int actVal = actStr.toInt();
          if (actVal == 7) {
            status.networkType = "4G";
          } else if (actVal == 2) {
            status.networkType = "3G";
          } else if (actVal == 0 || actVal == 1) {
            status.networkType = "2G";
          }
        }
      }
      break;
    }
    delay(10);
  }
}
