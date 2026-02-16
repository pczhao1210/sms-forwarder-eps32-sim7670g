#include "log_manager.h"
#include "config_manager.h"
#include "sms_storage.h"
#include "notification_manager.h"
#include "sms_filter.h"
#include "battery_manager.h"
#include "statistics_manager.h"
#include "i18n.h"
#include <map>
#include <vector>
#include <algorithm>
#include <SPIFFS.h>

// 数据结构定义
struct LongSMSInfo {
  int refNum;
  int totalParts;
  int currentPart;
};

struct LongSMSFragment {
  String content;
  int smsIndex;
  unsigned long timestamp;
};

struct TempSMSData {
  String sender;
  String rawContent;
  int smsIndex;
};

// 结构体定义
struct PDUInfo {
  String sender;
  int dcs;
  bool hasUDH;
  int ref;
  int total;
  int seq;
  String userData;
};

struct CMTData {
  int length;
  String pduHex;
};

// 前向声明
String extractSender(const String& rawData);
String extractSenderFromPDU(const String& pduData);
String extractRawContent(const String& rawData);
String extractContentFromPDU(const String& pduData);
String decodeUnicodeContent(const String& hexStr);
String decode7Bit(const String& hexData, int length);
bool isValidSMSContent(const String& content);
bool isLongSMSPDU(const String& pduData);
PDUInfo parsePDU(const String& pduData);
CMTData parseCMTData(const String& cmtData);
void handleCMTPDU(const String& pduHex);
void deleteSMS(int index);
void processSingleSMS(const String& sender, const String& content, int smsIndex);
bool isLongSMS(const String& pduData);
void handleLongSMSFragment(const String& sender, const String& rawContent, int smsIndex);
LongSMSInfo parseLongSMSInfo(const String& pduData);
void storeLongSMSFragment(const String& sender, const LongSMSInfo& info, const String& rawContent, int smsIndex);
bool needMoreFragments(const String& sender, int refNum, int totalParts);
void readMoreLongSMSFragments(int totalParts);
void assembleAndProcessLongSMS(const String& sender, int refNum);
void storeTempSMS(const String& sender, const String& rawContent, int smsIndex);
void clearTempSMSStorage();
void processBatchedSMS();
TempSMSData parseTempSMSLine(const String& line);
void startCMGLRead();
void processCMGLResponse(const String& response);
void processSingleCMGLEntry(const String& entry);
void processCompleteLongSMSGroup(const String& sender, int refNum, std::vector<TempSMSData>& fragments);
String extractCMTSender(const String& cmtData);
String extractCMTPDU(const String& cmtData);
void handleCMTSMS(const String& cmtData);

// 全局变量用于处理短信读取响应（已移动到sim7670g_manager.cpp）
extern bool waitingForSMSRead;
extern int currentSMSIndex;
extern String smsReadBuffer;

// CMGL超时处理变量（在sim7670g_manager.cpp中定义）
extern unsigned long cmglStartTime;
extern const unsigned long CMGL_TIMEOUT;
extern bool cmglReceiving;

// 批量读取短信的状态变量（已废弃，使用CMGL模式）
// int batchReadIndex = 0;
// int batchReadTotal = 0;

std::map<String, std::map<int, std::map<int, LongSMSFragment>>> longSMSBuffer;





// 处理原始短信数据
void handleRawSMSData(const String& rawData, int smsIndex) {
  LOGD("SMS_PARSE", "sms_parse_start", String(smsIndex).c_str(), String(rawData.length()).c_str());
  
  String sender = extractSender(rawData);
  String rawContent = extractRawContent(rawData);
  
  LOGD("SMS_PARSE", "sms_parse_sender_content", sender.c_str(), String(rawContent.length()).c_str());
  
  if (sender.isEmpty() || rawContent.isEmpty()) {
    LOGW("SMS_PARSE", "sms_parse_empty",
         sender.isEmpty() ? i18nGet("bool_yes") : i18nGet("bool_no"),
         rawContent.isEmpty() ? i18nGet("bool_yes") : i18nGet("bool_no"));
    return;
  }
  
  // 直接处理短信（CMGL模式下会通过临时文件处理）
  if (isLongSMS(rawContent)) {
    handleLongSMSFragment(sender, rawContent, smsIndex);
  } else {
    String content = decodeUnicodeContent(rawContent);
    processSingleSMS(sender, content, smsIndex);
  }
}

// 判断是否为长短信
bool isLongSMS(const String& pduData) {
  if (pduData.length() < 4) return false;
  
  // 获取PDU类型字节
  String pduType = pduData.substring(0, 2);
  int type = strtol(pduType.c_str(), NULL, 16);
  
  // 检查UDHI位 (bit6)
  return (type & 0x40) != 0;
}

// 处理长短信分片
void handleLongSMSFragment(const String& sender, const String& rawContent, int smsIndex) {
  // 解析长短信信息
  LongSMSInfo info = parseLongSMSInfo(rawContent);
  
  if (info.refNum == 0) {
    LOGW("LONG_SMS", "long_sms_parse_fail");
    deleteSMS(smsIndex);
    return;
  }
  
  // 存储分片
  storeLongSMSFragment(sender, info, rawContent, smsIndex);
  
  // 检查是否需要读取更多分片
  if (needMoreFragments(sender, info.refNum, info.totalParts)) {
    readMoreLongSMSFragments(info.totalParts);
  } else {
    // 所有分片已收集，拼接完整短信
    assembleAndProcessLongSMS(sender, info.refNum);
  }
}

// 解析长短信信息
LongSMSInfo parseLongSMSInfo(const String& pduData) {
  LongSMSInfo info = {0, 0, 0};
  
  // 简化解析：查找 05 00 03 模式
  int pos = pduData.indexOf("050003");
  if (pos >= 0 && pos + 12 <= pduData.length()) {
    String refStr = pduData.substring(pos + 6, pos + 8);
    String totalStr = pduData.substring(pos + 8, pos + 10);
    String currentStr = pduData.substring(pos + 10, pos + 12);
    
    info.refNum = strtol(refStr.c_str(), NULL, 16);
    info.totalParts = strtol(totalStr.c_str(), NULL, 16);
    info.currentPart = strtol(currentStr.c_str(), NULL, 16);
  }
  
  return info;
}

// 存储长短信分片
void storeLongSMSFragment(const String& sender, const LongSMSInfo& info, const String& rawContent, int smsIndex) {
  LongSMSFragment fragment;
  fragment.content = decodeUnicodeContent(rawContent);
  fragment.smsIndex = smsIndex;
  fragment.timestamp = millis();
  
  longSMSBuffer[sender][info.refNum][info.currentPart] = fragment;
  
  LOGI("LONG_SMS", "long_sms_store_fragment",
       String(info.currentPart).c_str(),
       String(info.totalParts).c_str(),
       String(info.refNum).c_str());
}

// 检查是否需要更多分片
bool needMoreFragments(const String& sender, int refNum, int totalParts) {
  if (longSMSBuffer.find(sender) == longSMSBuffer.end()) return true;
  if (longSMSBuffer[sender].find(refNum) == longSMSBuffer[sender].end()) return true;
  
  return longSMSBuffer[sender][refNum].size() < totalParts;
}

// 读取更多长短信分片
void readMoreLongSMSFragments(int totalParts) {
  // 长短信分片处理已移至批量模式
  LOGD("LONG_SMS", "long_sms_wait_more");
}

// 拼接并处理完整长短信
void assembleAndProcessLongSMS(const String& sender, int refNum) {
  if (longSMSBuffer.find(sender) == longSMSBuffer.end() || 
      longSMSBuffer[sender].find(refNum) == longSMSBuffer[sender].end()) {
    return;
  }
  
  auto& fragments = longSMSBuffer[sender][refNum];
  String fullContent = "";
  
  // 找到最大分片号并按序拼接
  int maxPart = 0;
  for (auto& frag : fragments) {
    if (frag.first > maxPart) maxPart = frag.first;
  }
  
  for (int i = 1; i <= maxPart; i++) {
    if (fragments.find(i) != fragments.end()) {
      fullContent += fragments[i].content;
    }
  }
  
  if (!fullContent.isEmpty()) {
    String timestamp = String(millis());
    
    // 存储完整长短信
    smsStorage.saveSMS(sender, fullContent, timestamp, true);
    
    // 输出处理完成日志
    LOGI("SMS", "sms_received_log", timestamp.c_str(), sender.c_str(), fullContent.c_str());
    
    // 删除所有分片
    for (auto& fragment : fragments) {
      deleteSMS(fragment.second.smsIndex);
    }
  }
  
  // 清理缓存
  longSMSBuffer[sender].erase(refNum);
  if (longSMSBuffer[sender].empty()) {
    longSMSBuffer.erase(sender);
  }
}

// 从临时文件处理长短信
void processLongSMSFromTemp(File& file) {
  String line;
  std::map<String, std::map<int, std::vector<TempSMSData>>> longSMSGroups;
  int longSMSCount = 0;
  
  // 第一遍：收集所有长短信分片
  while (file.available()) {
    line = file.readStringUntil('\n');
    TempSMSData data = parseTempSMSLine(line);
    
    if (data.sender.isEmpty()) continue;
    
    if (isLongSMS(data.rawContent)) {
      LongSMSInfo info = parseLongSMSInfo(data.rawContent);
      if (info.refNum > 0) {
        longSMSGroups[data.sender][info.refNum].push_back(data);
        longSMSCount++;
      }
    }
  }
  
  LOGI("SMS_BATCH", "sms_batch_long_count", String(longSMSCount).c_str());
  
  // 第二遍：处理完整的长短信组
  int processedGroups = 0;
  for (auto& senderGroup : longSMSGroups) {
    for (auto& refGroup : senderGroup.second) {
      processCompleteLongSMSGroup(senderGroup.first, refGroup.first, refGroup.second);
      processedGroups++;
    }
  }
  
  LOGI("SMS_BATCH", "sms_batch_long_groups", String(processedGroups).c_str());
}

// 从临时文件处理普通短信
void processNormalSMSFromTemp(File& file) {
  String line;
  int normalCount = 0;
  
  while (file.available()) {
    line = file.readStringUntil('\n');
    TempSMSData data = parseTempSMSLine(line);
    
    if (data.sender.isEmpty()) continue;
    
    if (!isLongSMS(data.rawContent)) {
      String content = decodeUnicodeContent(data.rawContent);
      String timestamp = String(millis());
      
      smsStorage.saveSMS(data.sender, content, timestamp, true);
      LOGI("SMS", "sms_received_log", timestamp.c_str(), data.sender.c_str(), content.c_str());
      deleteSMS(data.smsIndex);
      normalCount++;
    }
  }
  
  LOGI("SMS_BATCH", "sms_batch_normal_count", String(normalCount).c_str());
}

// 解析临时文件行
TempSMSData parseTempSMSLine(const String& line) {
  TempSMSData data;
  
  int firstPipe = line.indexOf('|');
  int secondPipe = line.indexOf('|', firstPipe + 1);
  
  if (firstPipe > 0 && secondPipe > firstPipe) {
    data.sender = line.substring(0, firstPipe);
    data.rawContent = line.substring(firstPipe + 1, secondPipe);
    data.smsIndex = line.substring(secondPipe + 1).toInt();
  }
  
  return data;
}

// 处理完整的长短信组
void processCompleteLongSMSGroup(const String& sender, int refNum, std::vector<TempSMSData>& fragments) {
  // 按分片号排序
  std::sort(fragments.begin(), fragments.end(), [](const TempSMSData& a, const TempSMSData& b) {
    LongSMSInfo infoA = parseLongSMSInfo(a.rawContent);
    LongSMSInfo infoB = parseLongSMSInfo(b.rawContent);
    return infoA.currentPart < infoB.currentPart;
  });
  
  String fullContent = "";
  for (auto& fragment : fragments) {
    String content = decodeUnicodeContent(fragment.rawContent);
    fullContent += content;
  }
  
  if (!fullContent.isEmpty()) {
    String timestamp = String(millis());
    smsStorage.saveSMS(sender, fullContent, timestamp, true);
    LOGI("SMS", "sms_received_log", timestamp.c_str(), sender.c_str(), fullContent.c_str());
    
    // 删除所有分片
    for (auto& fragment : fragments) {
      deleteSMS(fragment.smsIndex);
    }
  }
}

// 处理AT+CMGL="ALL"响应
void processCMGLResponse(const String& response) {
  LOGI("SMS_CMGL", "sms_cmgl_process_start", String(response.length()).c_str());
  
  // 清空临时存储
  clearTempSMSStorage();
  
  // 按行分割响应
  int startPos = 0;
  String currentSMS = "";
  int smsCount = 0;
  
  while (startPos < response.length()) {
    int lineEnd = response.indexOf('\n', startPos);
    if (lineEnd < 0) lineEnd = response.length();
    
    String line = response.substring(startPos, lineEnd);
    line.trim();
    
    if (line.startsWith("+CMGL:")) {
      // 处理前一条短信
      if (!currentSMS.isEmpty()) {
        processSingleCMGLEntry(currentSMS);
        smsCount++;
      }
      // 开始新的短信
      currentSMS = line + "\n";
    } else if (!line.isEmpty() && !line.equals("OK")) {
      // 短信内容行
      currentSMS += line + "\n";
    }
    
    startPos = lineEnd + 1;
  }
  
  // 处理最后一条短信
  if (!currentSMS.isEmpty()) {
    processSingleCMGLEntry(currentSMS);
    smsCount++;
  }
  
  LOGI("SMS_CMGL", "sms_cmgl_new_count", String(smsCount).c_str());
  
  // 处理收集到的短信
  processBatchedSMS();
}

// 处理单条CMGL条目
void processSingleCMGLEntry(const String& entry) {
  // 提取索引、发送方和内容
  int commaPos = entry.indexOf(',');
  if (commaPos <= 0) return;
  
  String indexStr = entry.substring(7, commaPos); // 跳过"+CMGL: "
  int smsIndex = indexStr.toInt();
  
  String sender = extractSender(entry);
  String rawContent = extractRawContent(entry);
  
  if (!sender.isEmpty() && !rawContent.isEmpty()) {
    LOGI("SMS_RAW", "sms_raw_entry", String(smsIndex).c_str(), sender.c_str(), rawContent.c_str());
    storeTempSMS(sender, rawContent, smsIndex);
  }
}

// 检查是否为长短信PDU
bool isLongSMSPDU(const String& pduData) {
  if (pduData.length() < 10) return false;
  
  // 检查是否包含长短信标识 050003 或 0800
  return (pduData.indexOf("050003") >= 0 || pduData.indexOf("0800") >= 0);
}

// 解析UDH并提取载荷数据
bool parseUdhAndExtractPayload(const String& hexLine, uint16_t &outRef, uint8_t &outTotal, uint8_t &outSeq, String &payloadHex) {
  PDUInfo info = parsePDU(hexLine);
  
  if (info.hasUDH && info.total > 1) {
    outRef = info.ref;
    outTotal = info.total;
    outSeq = info.seq;
    payloadHex = info.userData;
    
    LOGD("LONG_SMS", "long_sms_parsed", String(outRef).c_str(), String(outTotal).c_str(), String(outSeq).c_str());
    return true;
  }
  
  payloadHex = info.userData.isEmpty() ? hexLine : info.userData;
  return false;
}

// UCS2 BE解码（UTF-16BE 到 UTF-8）
String decodeUCS2BE(const String& hexData) {
  String result = "";
  
  // 转换hex字符串为字节数组
  std::vector<uint8_t> bytes;
  for (int i = 0; i < hexData.length() - 1; i += 2) {
    bytes.push_back(strtol(hexData.substring(i, i + 2).c_str(), NULL, 16));
  }
  
  // 按UTF-16BE解码
  for (int i = 0; i + 1 < bytes.size(); i += 2) {
    uint16_t unicode = (uint16_t(bytes[i]) << 8) | uint16_t(bytes[i + 1]);
    
    if (unicode == 0) continue;
    
    if (unicode <= 0x7F) {
      result += char(unicode);
    } else if (unicode <= 0x7FF) {
      result += char(0xC0 | ((unicode >> 6) & 0x1F));
      result += char(0x80 | (unicode & 0x3F));
    } else {
      result += char(0xE0 | ((unicode >> 12) & 0x0F));
      result += char(0x80 | ((unicode >> 6) & 0x3F));
      result += char(0x80 | (unicode & 0x3F));
    }
  }
  
  return result;
}

// 智能解码短信内容
String decodeUnicodeContent(const String& hexData) {
  if (hexData.isEmpty()) return "";
  
  // 首先尝试从完整PDU中提取内容
  String pduContent = extractContentFromPDU(hexData);
  if (!pduContent.isEmpty()) {
    return pduContent;
  }
  
  // 如果PDU解析失败，尝试直接UCS2解码
  String ucs2Result = decodeUCS2BE(hexData);
  if (!ucs2Result.isEmpty()) {
    return ucs2Result;
  }
  
  // 最后尝试7bit解码
  return decode7Bit(hexData, hexData.length() / 2);
}

// 验证短信内容是否有效（非乱码）
bool isValidSMSContent(const String& content) {
  if (content.isEmpty()) {
    LOGD("SMS_VALID", "sms_valid_empty");
    return false;
  }
  
  int validChars = 0;
  int totalChars = content.length();
  int chineseChars = 0;
  
  for (int i = 0; i < totalChars; i++) {
    unsigned char c = content.charAt(i);
    // 检查是否为可打印字符或常见空白字符
    if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t') {
      validChars++;
    } else if (c >= 0x80) {
      // UTF-8中文字符
      validChars++;
      if (c >= 0xE4 && c <= 0xE9) chineseChars++; // 中文字符范围
    }
  }
  
  float validRatio = (float)validChars / totalChars;
  bool isValid = validRatio >= 0.3; // 降低阈值到30%
  
  LOGD("SMS_VALID", "sms_valid_stats",
       String(totalChars).c_str(),
       String(validChars).c_str(),
       String(validRatio * 100, 1).c_str(),
       isValid ? i18nGet("sms_valid_ok") : i18nGet("sms_valid_garbled"));
  
  return isValid;
}

// 处理单条短信
void processSingleSMS(const String& sender, const String& content, int smsIndex) {
  String timestamp = String(millis());
  
  statisticsManager.incrementSMSReceived();
  statisticsManager.updateLastSMS(sender);
  sleepManager.updateActivity();
  
  // 验证内容有效性
  if (!isValidSMSContent(content)) {
    LOGW("SMS", "sms_garbled_skip", sender.c_str());
    smsStorage.saveSMS(sender, i18nFormat("sms_garbled_filtered"), timestamp, false);
    deleteSMS(smsIndex);
    return;
  }
  
  // 应用短信过滤器
  if (!smsFilter.shouldForwardSMS(sender, content)) {
    LOGI("SMS", "sms_filtered", sender.c_str());
    statisticsManager.incrementSMSFiltered();
    smsStorage.saveSMS(sender, content, timestamp, false);
    deleteSMS(smsIndex);
    return;
  }
  
  // 存储短信
  smsStorage.saveSMS(sender, content, timestamp, true);
  
  // 输出处理完成日志
  LOGI("SMS", "sms_received_log", timestamp.c_str(), sender.c_str(), content.c_str());
  
  // 转发短信
  statisticsManager.incrementSMSForwarded();
  notificationManager.forwardSMS(sender, content);
  
  // 删除已读短信
  deleteSMS(smsIndex);
}

// 从PDU数据中提取发送方号码
String extractSenderFromPDU(const String& pduData) {
  if (pduData.length() < 20) return "";
  
  try {
    // 跳过SMSC长度和地址
    int pos = 0;
    int smscLen = strtol(pduData.substring(pos, pos + 2).c_str(), NULL, 16);
    pos += 2 + smscLen * 2;
    
    if (pos + 2 > pduData.length()) return "";
    
    // 跳过PDU类型
    pos += 2;
    
    if (pos + 2 > pduData.length()) return "";
    
    // 读取发送方地址长度
    int senderLen = strtol(pduData.substring(pos, pos + 2).c_str(), NULL, 16);
    pos += 2;
    
    if (pos + 2 > pduData.length()) return "";
    
    // 读取发送方地址类型
    int addrType = strtol(pduData.substring(pos, pos + 2).c_str(), NULL, 16);
    pos += 2;
    
    // 计算发送方号码的实际字节数
    int senderBytes = (senderLen + 1) / 2;
    if (pos + senderBytes * 2 > pduData.length()) return "";
    
    // 提取并解码发送方号码
    String senderHex = pduData.substring(pos, pos + senderBytes * 2);
    String sender = "";
    
    if ((addrType & 0x70) == 0x50) {
      // 字母数字格式，需要7bit解码
      // 简化处理，直接返回hex
      sender = senderHex;
    } else {
      // 数字格式，交换每对数字
      for (int i = 0; i < senderHex.length(); i += 2) {
        if (i + 1 < senderHex.length()) {
          char c1 = senderHex.charAt(i + 1);
          char c2 = senderHex.charAt(i);
          if (c1 != 'F' && c1 != 'f') sender += c1;
          if (c2 != 'F' && c2 != 'f') sender += c2;
        }
      }
    }
    
    return sender;
  } catch (...) {
    return "";
  }
}

// 提取发送方号码
String extractSender(const String& rawData) {
  // 查找+CMGR:行
  int cmgrPos = rawData.indexOf("+CMGR:");
  if (cmgrPos >= 0) {
    String headerLine = rawData.substring(cmgrPos);
    int newlinePos = headerLine.indexOf('\n');
    if (newlinePos > 0) headerLine = headerLine.substring(0, newlinePos);
    
    // 解析+CMGR: "REC READ","10086900","","25/08/25,10:23:55+32"
    // 第二个引号对是发送方号码
    int firstQuote = headerLine.indexOf('"');
    if (firstQuote >= 0) {
      int secondQuote = headerLine.indexOf('"', firstQuote + 1);
      if (secondQuote > firstQuote) {
        int thirdQuote = headerLine.indexOf('"', secondQuote + 1);
        if (thirdQuote >= 0) {
          int fourthQuote = headerLine.indexOf('"', thirdQuote + 1);
          if (fourthQuote > thirdQuote) {
            return headerLine.substring(thirdQuote + 1, fourthQuote);
          }
        }
      }
    }
  }
  
  // 如果文本模式解析失败，尝试从PDU数据中提取
  String rawContent = extractRawContent(rawData);
  if (!rawContent.isEmpty()) {
    return extractSenderFromPDU(rawContent);
  }
  
  return "";
}

// 解析完整PDU并提取用户数据
PDUInfo parsePDU(const String& pduData) {
  PDUInfo info = {"", 0, false, 0, 0, 0, ""};
  if (pduData.length() < 20) return info;
  
  try {
    // 转换为字节数组
    std::vector<uint8_t> p;
    for (int i = 0; i < pduData.length() - 1; i += 2) {
      p.push_back(strtol(pduData.substring(i, i + 2).c_str(), NULL, 16));
    }
    
    if (p.size() < 10) return info;
    
    int idx = 0;
    
    // SMSC长度和地址
    uint8_t smscLen = p[idx++];
    idx += smscLen; // 跳过SMSC
    
    if (idx >= p.size()) return info;
    
    // PDU类型
    uint8_t pduType = p[idx++];
    info.hasUDH = (pduType & 0x40) != 0;
    
    if (idx >= p.size()) return info;
    
    // 发送方地址
    uint8_t senderLen = p[idx++]; // 数字个数
    if (idx >= p.size()) return info;
    
    uint8_t toa = p[idx++]; // 地址类型
    int senderBytes = (senderLen + 1) / 2;
    
    if (idx + senderBytes > p.size()) return info;
    
    // 解码BCD号码
    for (int i = 0; i < senderBytes && info.sender.length() < senderLen; i++) {
      uint8_t b = p[idx + i];
      uint8_t lo = b & 0x0F;
      uint8_t hi = (b >> 4) & 0x0F;
      if (info.sender.length() < senderLen) info.sender += char('0' + lo);
      if (hi != 0x0F && info.sender.length() < senderLen) info.sender += char('0' + hi);
    }
    idx += senderBytes;
    
    if (idx + 9 > p.size()) return info; // PID + DCS + SCTS
    
    // 跳过PID
    idx++;
    
    // DCS
    info.dcs = p[idx++];
    
    // 跳过SCTS（7字节）
    idx += 7;
    
    if (idx >= p.size()) return info;
    
    // UDL
    uint8_t udl = p[idx++];
    
    if (idx + udl > p.size()) return info;
    
    // 用户数据
    const uint8_t* ud = &p[idx];
    const uint8_t* userData = ud;
    int userDataLen = udl;
    
    // 处理UDH
    if (info.hasUDH && udl > 0) {
      uint8_t udhl = ud[0];
      if (1 + udhl <= udl) {
        userData = ud + 1 + udhl; // 跳过UDH
        userDataLen = udl - 1 - udhl;
        
        // 解析UDH中的长短信标识
        int pos = 1;
        while (pos + 1 < 1 + udhl) {
          uint8_t iei = ud[pos];
          uint8_t iedl = ud[pos + 1];
          if (pos + 1 + iedl > udhl) break;
          
          if (iei == 0x00 && iedl == 0x03 && pos + 4 < 1 + udhl) {
            // 8-bit ref
            info.ref = ud[pos + 2];
            info.total = ud[pos + 3];
            info.seq = ud[pos + 4];
            break;
          } else if (iei == 0x08 && iedl == 0x04 && pos + 5 < 1 + udhl) {
            // 16-bit ref
            info.ref = (uint16_t(ud[pos + 2]) << 8) | ud[pos + 3];
            info.total = ud[pos + 4];
            info.seq = ud[pos + 5];
            break;
          }
          pos += 2 + iedl;
        }
      }
    }
    
    // 转换用户数据为hex字符串
    for (int i = 0; i < userDataLen; i++) {
      char hex[3];
      sprintf(hex, "%02X", userData[i]);
      info.userData += hex;
    }
    
  } catch (...) {
    // 解析失败
  }
  
  return info;
}

// 从PDU数据中提取短信内容
String extractContentFromPDU(const String& pduData) {
  PDUInfo info = parsePDU(pduData);
  if (info.userData.isEmpty()) return "";
  
  // 根据DCS解码
  if (info.dcs == 0x08) {
    return decodeUCS2BE(info.userData);
  } else {
    return decode7Bit(info.userData, info.userData.length() / 2);
  }
}

// 7bit解码函数
String decode7Bit(const String& hexData, int length) {
  if (hexData.length() == 0 || length == 0) return "";
  
  std::vector<uint8_t> bytes;
  for (int i = 0; i < hexData.length() - 1; i += 2) {
    bytes.push_back(strtol(hexData.substring(i, i + 2).c_str(), NULL, 16));
  }
  
  String result = "";
  int bitOffset = 0;
  
  for (int i = 0; i < length && i < bytes.size() * 8 / 7; i++) {
    int byteIndex = (i * 7 + bitOffset) / 8;
    int bitIndex = (i * 7 + bitOffset) % 8;
    
    if (byteIndex >= bytes.size()) break;
    
    uint8_t char7bit = 0;
    if (bitIndex <= 1) {
      char7bit = (bytes[byteIndex] >> bitIndex) & 0x7F;
    } else {
      char7bit = (bytes[byteIndex] >> bitIndex) & (0x7F >> (bitIndex - 1));
      if (byteIndex + 1 < bytes.size()) {
        char7bit |= (bytes[byteIndex + 1] << (8 - bitIndex)) & 0x7F;
      }
    }
    
    if (char7bit > 0) {
      result += (char)char7bit;
    }
  }
  
  return result;
}

// 提取原始内容
String extractRawContent(const String& rawData) {
  int cmgrPos = rawData.indexOf("+CMGR:");
  if (cmgrPos >= 0) {
    int lineEnd = rawData.indexOf('\n', cmgrPos);
    if (lineEnd >= 0) {
      int nextLineStart = lineEnd + 1;
      int nextLineEnd = rawData.indexOf('\n', nextLineStart);
      if (nextLineEnd < 0) nextLineEnd = rawData.indexOf("OK", nextLineStart);
      if (nextLineEnd < 0) nextLineEnd = rawData.length();
      
      String nextLine = rawData.substring(nextLineStart, nextLineEnd);
      nextLine.trim();
      
      if (nextLine.startsWith("\"") && nextLine.endsWith("\"")) {
        nextLine = nextLine.substring(1, nextLine.length() - 1);
      }
      
      // 只检查是否为有效hex字符串（移除'0'开头限制）
      if (nextLine.length() > 0) {
        return nextLine;
      }
    }
  }
  return "";
}

// 删除已读短信
void deleteSMS(int index) {
  if (index <= 0) {
    LOGD("SMS_DEL", "sms_delete_skip_cmt", String(index).c_str());
    return;
  }
  
  extern HardwareSerial sim7670g;
  String cmd = "AT+CMGD=" + String(index);
  LOGD("SMS_DEL", "sms_delete", String(index).c_str());
  
  if (config.debug.atCommandEcho) {
    logManager.addLog(LOG_DEBUG, "AT_TX", cmd);
  }
  
  sim7670g.println(cmd);
  sim7670g.flush();
}





// 使用AT+CMGL="ALL"读取所有短信
void startCMGLRead() {
  extern HardwareSerial sim7670g;
  
  LOGI("SMS_CMGL", "sms_cmgl_start_all");
  
  // 清空临时存储和缓冲区
  clearTempSMSStorage();
  smsReadBuffer = "";
  
  // 发送AT+CMGL="ALL"指令
  if (config.debug.atCommandEcho) {
    logManager.addLog(LOG_DEBUG, "AT_TX", "AT+CMGL=\"ALL\"");
  }
  
  sim7670g.println("AT+CMGL=\"ALL\"");
  sim7670g.flush();
  
  waitingForSMSRead = true;
  currentSMSIndex = -1; // 标记为CMGL模式
  cmglReceiving = false;
  cmglStartTime = 0;
}

// 临时存储短信到flash
void storeTempSMS(const String& sender, const String& rawContent, int smsIndex) {
  String tempData = sender + "|" + rawContent + "|" + String(smsIndex) + "\n";
  
  File file = SPIFFS.open("/temp_sms.txt", "a");
  if (file) {
    file.print(tempData);
    file.close();
  }
  
  // 已存储到临时文件
  LOGD("SMS_TEMP", "sms_temp_stored", String(smsIndex).c_str());
}

// 从CMGR响应存储临时短信
void storeTempSMSFromCMGR(const String& rawData, int smsIndex) {
  String sender = extractSender(rawData);
  String rawContent = extractRawContent(rawData);
  
  if (!sender.isEmpty() && !rawContent.isEmpty()) {
    storeTempSMS(sender, rawContent, smsIndex);
    LOGD("SMS_CMGR", "sms_cmgr_store", String(smsIndex).c_str(), sender.c_str());
  } else {
    LOGW("SMS_CMGR", "sms_cmgr_parse_fail", String(smsIndex).c_str());
  }
}

// 验证PDU长度
bool validatePduLength(const String &pduHex, int tpduLength) {
  String pdu = pduHex;
  pdu.trim();
  if (pdu.length() < 2) return false; // 至少要有1字节SMSC长度
  int smscLen = strtol(pdu.substring(0, 2).c_str(), NULL, 16);
  int expected = (1 + smscLen + tpduLength) * 2; // 全部字节转成HEX字符数
  return pdu.length() == expected;
}

// 解析CMT数据（分离第一行和纯hex第二行）
CMTData parseCMTData(const String& cmtData) {
  CMTData result = {0, ""};
  
  // 查找+CMT:行
  int cmtPos = cmtData.indexOf("+CMT:");
  if (cmtPos < 0) return result;
  
  // 提取第一行（+CMT行）
  int firstLineEnd = cmtData.indexOf('\n', cmtPos);
  if (firstLineEnd < 0) return result;
  
  String cmtLine = cmtData.substring(cmtPos, firstLineEnd);
  
  // 从+CMT行提取length参数（最后一个数字）
  int lastComma = cmtLine.lastIndexOf(',');
  if (lastComma > 0) {
    String lengthStr = cmtLine.substring(lastComma + 1);
    lengthStr.trim();
    result.length = lengthStr.toInt();
  }
  
  // 提取第二行（纯hex PDU数据）
  int secondLineStart = firstLineEnd + 1;
  int secondLineEnd = cmtData.indexOf('\n', secondLineStart);
  if (secondLineEnd < 0) secondLineEnd = cmtData.length();
  
  String pduLine = cmtData.substring(secondLineStart, secondLineEnd);
  pduLine.trim();
  
  // 验证PDU数据长度
  if (result.length > 0) {
    if (validatePduLength(pduLine, result.length)) {
      result.pduHex = pduLine;
      LOGD("CMT_PARSE", "cmt_pdu_length_ok", String(result.length).c_str(), String(pduLine.length()).c_str());
    } else {
      LOGW("CMT_PARSE", "cmt_pdu_length_fail", String(result.length).c_str(), String(pduLine.length()).c_str());
      result.pduHex = pduLine; // 仍然使用数据，但记录警告
    }
  } else {
    // 如果无法从CMT行解析长度，直接使用PDU数据
    result.pduHex = pduLine;
    LOGD("CMT_PARSE", "cmt_pdu_length_full", String(pduLine.length()).c_str());
  }
  
  return result;
}

// 长短信会话管理
struct CMTSession {
  bool used;
  String key;
  uint16_t ref;
  String sender;
  uint8_t total;
  uint8_t received;
  String parts[12];
  unsigned long lastSeen;
};

CMTSession cmtSessions[6];

// 存储分片并尝试拼接
String storeSegmentAndAssemble(const String& sender, uint16_t ref, uint8_t total, uint8_t seq, const String& payloadHex) {
  String key = sender + ":" + String(ref);
  int idx = -1;
  
  // 找已有会话
  for (int i = 0; i < 6; ++i) {
    if (cmtSessions[i].used && cmtSessions[i].key == key) {
      idx = i;
      break;
    }
  }
  
  // 找空槽
  if (idx == -1) {
    for (int i = 0; i < 6; ++i) {
      if (!cmtSessions[i].used) {
        idx = i;
        break;
      }
    }
  }
  
  if (idx == -1) return ""; // 无可用槽
  
  // 初始化新会话
  if (!cmtSessions[idx].used) {
    cmtSessions[idx].used = true;
    cmtSessions[idx].key = key;
    cmtSessions[idx].ref = ref;
    cmtSessions[idx].sender = sender;
    cmtSessions[idx].total = total;
    cmtSessions[idx].received = 0;
    for (int j = 0; j < 12; ++j) cmtSessions[idx].parts[j] = "";
  }
  
  // 存储分片
  if (seq == 0 || seq > 12) return "";
  if (cmtSessions[idx].parts[seq-1].length() == 0) {
    cmtSessions[idx].parts[seq-1] = payloadHex;
    cmtSessions[idx].received++;
  }
  cmtSessions[idx].lastSeen = millis();
  
  // 检查是否收齐
  if (cmtSessions[idx].received >= cmtSessions[idx].total) {
    String full = "";
    for (int p = 0; p < cmtSessions[idx].total; ++p) {
      full += cmtSessions[idx].parts[p];
    }
    // 释放会话
    cmtSessions[idx].used = false;
    return full;
  }
  
  return ""; // 未收齐
}

// 处理CMT PDU数据
void handleCMTPDU(const String& pduHex) {
  // 从PDU解析所有信息
  PDUInfo info = parsePDU(pduHex);
  
  if (info.sender.isEmpty()) {
    LOGW("SMS_CMT", "sms_cmt_sender_parse_fail");
    return;
  }
  
  LOGD("SMS_CMT", "sms_cmt_parsed", info.sender.c_str(), String(info.dcs, HEX).c_str());
  
  // 处理长短信或普通短信
  if (info.hasUDH && info.total > 1) {
    LOGI("SMS_CMT", "sms_cmt_long_fragment",
         info.sender.c_str(),
         String(info.ref).c_str(),
         String(info.seq).c_str(),
         String(info.total).c_str());
    
    String assembled = storeSegmentAndAssemble(info.sender, info.ref, info.total, info.seq, info.userData);
    if (!assembled.isEmpty()) {
      String content = (info.dcs == 0x08) ? decodeUCS2BE(assembled) : decode7Bit(assembled, assembled.length() / 2);
      processSingleSMS(info.sender, content, 0);
    }
  } else {
    String content = (info.dcs == 0x08) ? decodeUCS2BE(info.userData) : decode7Bit(info.userData, info.userData.length() / 2);
    processSingleSMS(info.sender, content, 0);
  }
}

// 处理CMT短信数据
void handleCMTSMS(const String& cmtData) {
  CMTData cmt = parseCMTData(cmtData);
  
  if (cmt.pduHex.isEmpty()) {
    LOGW("SMS_CMT", "sms_cmt_parse_fail");
    return;
  }
  
  // 从PDU解析所有信息
  PDUInfo info = parsePDU(cmt.pduHex);
  
  if (info.sender.isEmpty()) {
    LOGW("SMS_CMT", "sms_cmt_sender_parse_fail");
    return;
  }
  
  LOGD("SMS_CMT", "sms_cmt_parsed", info.sender.c_str(), String(info.dcs, HEX).c_str());
  
  // 处理长短信或普通短信
  if (info.hasUDH && info.total > 1) {
    LOGI("SMS_CMT", "sms_cmt_long_fragment",
         info.sender.c_str(),
         String(info.ref).c_str(),
         String(info.seq).c_str(),
         String(info.total).c_str());
    
    String assembled = storeSegmentAndAssemble(info.sender, info.ref, info.total, info.seq, info.userData);
    if (!assembled.isEmpty()) {
      String content = (info.dcs == 0x08) ? decodeUCS2BE(assembled) : decode7Bit(assembled, assembled.length() / 2);
      processSingleSMS(info.sender, content, 0);
    }
  } else {
    String content = (info.dcs == 0x08) ? decodeUCS2BE(info.userData) : decode7Bit(info.userData, info.userData.length() / 2);
    processSingleSMS(info.sender, content, 0);
  }
}

// CMT短信缓存
std::vector<String> pendingCMTMessages;

// 存储待处理的CMT短信（纯PDU hex）
void storePendingCMTSMS(const String& pduHex) {
  pendingCMTMessages.push_back(pduHex);
  LOGD("SMS_CMT", "sms_cmt_pending_count", String(pendingCMTMessages.size()).c_str());
}

// 处理所有待处理的CMT短信
void processPendingCMTSMS() {
  LOGI("SMS_CMT", "sms_cmt_process_start", String(pendingCMTMessages.size()).c_str());
  
  for (const String& pduHex : pendingCMTMessages) {
    handleCMTPDU(pduHex); // 直接处理PDU
  }
  
  pendingCMTMessages.clear();
}



// 清空临时存储
void clearTempSMSStorage() {
  SPIFFS.remove("/temp_sms.txt");
}

// 处理所有批量读取的短信
void processBatchedSMS() {
  LOGI("SMS_BATCH", "sms_batch_start");
  
  File file = SPIFFS.open("/temp_sms.txt", "r");
  if (!file) {
    LOGW("SMS_BATCH", "sms_batch_temp_missing");
    return;
  }
  
  // 检查文件大小
  size_t fileSize = file.size();
  LOGI("SMS_BATCH", "sms_batch_temp_size", String(fileSize).c_str());
  
  // 分两阶段：1.处理长短信 2.处理普通短信
  processLongSMSFromTemp(file);
  file.seek(0);
  processNormalSMSFromTemp(file);
  
  file.close();
  SPIFFS.remove("/temp_sms.txt");
  
  LOGI("SMS_BATCH", "sms_batch_done");
}
