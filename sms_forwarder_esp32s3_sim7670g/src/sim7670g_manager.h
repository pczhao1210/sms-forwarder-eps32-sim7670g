#ifndef SIM7670G_MANAGER_H
#define SIM7670G_MANAGER_H

#include <HardwareSerial.h>

// SIM7670G控制引脚 (根据实际GPIO连接)
#define SIM7670G_PWR_PIN    16  // PWR_KEY电源控制
#define SIM7670G_RESET_PIN  15  // RESET复位控制
#define SIM7670G_FLIGHT_PIN 14  // FLIGHT飞行模式
#define SIM7670G_RI_PIN     40  // RI振铃指示
#define SIM7670G_DTR_PIN    45  // DTR数据终端就绪

// UART通信引脚
#define SIM7670G_RX_PIN     17  // ESP32 RX <- SIM7670G TX
#define SIM7670G_TX_PIN     18  // ESP32 TX -> SIM7670G RX

// USB引脚 (SIM7670G)
#define SIM7670G_USB_DN     19  // USB D-
#define SIM7670G_USB_DP     20  // USB D+

// 其他硬件引脚定义
#define RGB_LED_PIN         38  // WS2812B RGB LED
#define USER_LED_PIN        1   // 用户LED
#define I2C_SDA_PIN         3   // MAX17048 SDA
#define I2C_SCL_PIN         2   // MAX17048 SCL

// TF卡SDMMC接口 (修正)
#define SDMMC_CLK           5   // 时钟线
#define SDMMC_CMD           4   // 命令线
#define SDMMC_DATA          6   // 数据线
#define SD_CD_PIN           46  // 卡检测引脚

extern HardwareSerial sim7670g;

void initSIM7670G();
void powerOnSIM7670G();
String sendATCommand(const String& command);
String getATCommandDescription(const String& command);
bool checkSIMCardInserted();
bool checkSIMStatus();
void resetSIMCheck();
bool checkNetworkStatus();
int getSignalStrength();
bool initGNSS();
String getGNSSData();
void simTask();
void handleUartRx();
void sendNetworkConfig();
void testNetworkConnectivity();
void readSMSByIndex(int index);
bool sendSMS(const String& phoneNumber, const String& message);
void checkSMSNotificationConfig();
void checkAllSMS();

// 短信处理函数声明（在sms_handler.cpp中实现）
void handleRawSMSData(const String& rawData, int smsIndex);
void deleteSMS(int index);
void storeTempSMSFromCMGR(const String& rawData, int smsIndex);
void clearTempSMSStorage();
void processBatchedSMS();
void handleCMTSMS(const String& cmtData);
void handleCMTPDU(const String& pduHex);
void storePendingCMTSMS(const String& pduHex);
void processPendingCMTSMS();

// 导出SimState
enum SimState {
  SIM_STATE_IDLE,
  SIM_STATE_POWER_ON,
  SIM_STATE_WAIT_BOOT,
  SIM_STATE_WAIT_AT_OK,
  SIM_STATE_INIT_CMDS,
  SIM_STATE_CONFIG_APN,
  SIM_STATE_READY
};

// 短信信息结构
struct SMSMessage {
  String sender;
  String content;
  String timestamp;
  int index;
};

extern SimState simState;

// 系统状态管理
struct SystemStatus {
  int signalStrength = -999;
  bool simReady = false;
  bool networkConnected = false;
  bool csRegistered = false;
  bool epsRegistered = false;
  bool dataAttached = false;
  String operatorName = "Unknown";
  String operatorCode = "Unknown";
  String homeOperatorName = "Unknown";
  String homeOperatorCode = "Unknown";
  String networkType = "Unknown";
  bool isRoaming = false;
  unsigned long lastUpdate = 0;
  bool initialized = false;
};

class SystemStatusManager {
public:
  static void initStatus();
  static void updateStatus();
  static SystemStatus getStatus();
  static bool needsUpdate();
  static void refreshSignalOnly();
  static void refreshAllStatus();
  
private:
  static SystemStatus status;
  static void queryAllStatus();
  static void querySignalStrength();
  static void querySIMStatus();
  static void queryNetworkStatus();
  static void queryDataStatus();
  static void queryOperatorInfo();
  static void parseOperatorResponse(const String& line);
};

extern SystemStatusManager systemStatus;



#endif
