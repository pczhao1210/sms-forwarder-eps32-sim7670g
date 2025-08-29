#include "log_manager.h"
#include "config_manager.h"
#include "sms_storage.h"
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

// 前向声明
String extractSender(const String& rawData);
String extractRawContent(const String& rawData);
String decodeUnicodeContent(const String& hexStr);
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
  logManager.addLog(LOG_DEBUG, "SMS_PARSE", "开始解析索引 " + String(smsIndex) + ", 数据长度: " + String(rawData.length()));
  
  String sender = extractSender(rawData);
  String rawContent = extractRawContent(rawData);
  
  logManager.addLog(LOG_DEBUG, "SMS_PARSE", "Sender: '" + sender + "', Content length: " + String(rawContent.length()));
  
  if (sender.isEmpty() || rawContent.isEmpty()) {
    logManager.addLog(LOG_WARN, "SMS_PARSE", "无法解析短信数据 - Sender empty: " + String(sender.isEmpty()) + ", Content empty: " + String(rawContent.isEmpty()));
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
    logManager.addLog(LOG_WARN, "LONG_SMS", "无法解析长短信信息");
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
  
  logManager.addLog(LOG_INFO, "LONG_SMS", "存储分片 " + String(info.currentPart) + "/" + String(info.totalParts) + " 参考号:" + String(info.refNum));
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
  logManager.addLog(LOG_DEBUG, "LONG_SMS", "等待更多分片");
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
    logManager.addLog(LOG_INFO, "SMS", "收到短信，时间" + timestamp + ",发件人" + sender + "， 内容" + fullContent);
    
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
  
  logManager.addLog(LOG_INFO, "SMS_BATCH", "发现 " + String(longSMSCount) + " 个长短信分片");
  
  // 第二遍：处理完整的长短信组
  int processedGroups = 0;
  for (auto& senderGroup : longSMSGroups) {
    for (auto& refGroup : senderGroup.second) {
      processCompleteLongSMSGroup(senderGroup.first, refGroup.first, refGroup.second);
      processedGroups++;
    }
  }
  
  logManager.addLog(LOG_INFO, "SMS_BATCH", "处理了 " + String(processedGroups) + " 个长短信组");
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
      logManager.addLog(LOG_INFO, "SMS", "收到短信，时间" + timestamp + ",发件人" + data.sender + "， 内容" + content);
      deleteSMS(data.smsIndex);
      normalCount++;
    }
  }
  
  logManager.addLog(LOG_INFO, "SMS_BATCH", "处理了 " + String(normalCount) + " 条普通短信");
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
    logManager.addLog(LOG_INFO, "SMS", "收到短信，时间" + timestamp + ",发件人" + sender + "， 内容" + fullContent);
    
    // 删除所有分片
    for (auto& fragment : fragments) {
      deleteSMS(fragment.smsIndex);
    }
  }
}

// 处理AT+CMGL="ALL"响应
void processCMGLResponse(const String& response) {
  logManager.addLog(LOG_INFO, "SMS_CMGL", "开始处理CMGL响应，长度: " + String(response.length()));
  
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
  
  logManager.addLog(LOG_INFO, "SMS_CMGL", "发现 " + String(smsCount) + " 条新短信");
  
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
    logManager.addLog(LOG_INFO, "SMS_RAW", "索引" + String(smsIndex) + ", 发送方: " + sender + ", 数据: " + rawContent);
    storeTempSMS(sender, rawContent, smsIndex);
  }
}

// 解析UDH并提取载荷数据
bool parseUdhAndExtractPayload(const String& hexLine, uint16_t &outRef, uint8_t &outTotal, uint8_t &outSeq, String &payloadHex) {
  if (hexLine.length() < 2) return false;
  
  // 转换为字节数组
  std::vector<uint8_t> buf;
  for (int i = 0; i < hexLine.length() - 1; i += 2) {
    String hex2 = hexLine.substring(i, i + 2);
    buf.push_back(strtol(hex2.c_str(), NULL, 16));
  }
  
  if (buf.size() == 0) return false;
  
  // 首字节是UDL（UDH长度）
  uint8_t udl = buf[0];
  if (udl == 0 || udl + 1 > buf.size()) {
    payloadHex = hexLine;
    return false;
  }
  
  // 解析UDH内的IE
  int pos = 1;
  bool foundConcat = false;
  while (pos < 1 + udl) {
    if (pos >= buf.size()) break;
    uint8_t iei = buf[pos++];
    if (pos >= buf.size()) break;
    uint8_t iedl = buf[pos++];
    if (pos + iedl > 1 + udl || pos + iedl > buf.size()) break;
    
    if (iei == 0x00 && iedl == 3) {
      // 8-bit ref
      outRef = buf[pos];
      outTotal = buf[pos + 1];
      outSeq = buf[pos + 2];
      foundConcat = true;
    } else if (iei == 0x08 && iedl == 4) {
      // 16-bit ref
      outRef = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
      outTotal = buf[pos + 2];
      outSeq = buf[pos + 3];
      foundConcat = true;
    }
    pos += iedl;
  }
  
  // 提取载荷数据
  int payloadOffset = 1 + udl;
  payloadHex = "";
  for (int i = payloadOffset; i < buf.size(); ++i) {
    char tmp[3];
    sprintf(tmp, "%02X", buf[i]);
    payloadHex += tmp;
  }
  
  return foundConcat;
}

// UCS2 BE解码
String decodeUCS2BE(const String& hexData) {
  String result = "";
  
  for (int i = 0; i < hexData.length() - 3; i += 4) {
    String hex4 = hexData.substring(i, i + 4);
    uint16_t unicode = strtoul(hex4.c_str(), NULL, 16);
    
    if (unicode == 0) continue;
    
    if (unicode < 0x80) {
      result += (char)unicode;
    } else if (unicode < 0x800) {
      result += (char)(0xC0 | (unicode >> 6));
      result += (char)(0x80 | (unicode & 0x3F));
    } else {
      result += (char)(0xE0 | (unicode >> 12));
      result += (char)(0x80 | ((unicode >> 6) & 0x3F));
      result += (char)(0x80 | (unicode & 0x3F));
    }
  }
  
  return result;
}

// 简化版本，直接解码
String decodeUnicodeContent(const String& hexData) {
  return decodeUCS2BE(hexData);
}

// 处理单条短信
void processSingleSMS(const String& sender, const String& content, int smsIndex) {
  String timestamp = String(millis());
  
  // 简化处理：直接存储和转发
  smsStorage.saveSMS(sender, content, timestamp, true);
  
  // 输出处理完成日志
  logManager.addLog(LOG_INFO, "SMS", "收到短信，时间" + timestamp + ",发件人" + sender + "， 内容" + content);
  
  // 删除已读短信
  deleteSMS(smsIndex);
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
  return "";
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
    logManager.addLog(LOG_DEBUG, "SMS_DEL", "CMT短信无需删除，索引: " + String(index));
    return;
  }
  
  extern HardwareSerial sim7670g;
  String cmd = "AT+CMGD=" + String(index);
  logManager.addLog(LOG_DEBUG, "SMS_DEL", "删除短信: " + String(index));
  
  if (config.debug.atCommandEcho) {
    logManager.addLog(LOG_DEBUG, "AT_TX", cmd);
  }
  
  sim7670g.println(cmd);
  sim7670g.flush();
}





// 使用AT+CMGL="ALL"读取所有短信
void startCMGLRead() {
  extern HardwareSerial sim7670g;
  
  logManager.addLog(LOG_INFO, "SMS_CMGL", "开始使用CMGL读取所有短信");
  
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
  logManager.addLog(LOG_DEBUG, "SMS_TEMP", "已存储短信索引: " + String(smsIndex));
}

// 从CMGR响应存储临时短信
void storeTempSMSFromCMGR(const String& rawData, int smsIndex) {
  String sender = extractSender(rawData);
  String rawContent = extractRawContent(rawData);
  
  if (!sender.isEmpty() && !rawContent.isEmpty()) {
    storeTempSMS(sender, rawContent, smsIndex);
    logManager.addLog(LOG_DEBUG, "SMS_CMGR", "存储CMGR短信索引 " + String(smsIndex) + ", 发送方: " + sender);
  } else {
    logManager.addLog(LOG_WARN, "SMS_CMGR", "无法解析CMGR短信索引 " + String(smsIndex));
  }
}

// 提取CMT发送方
String extractCMTSender(const String& cmtData) {
  int firstQuote = cmtData.indexOf('"');
  if (firstQuote >= 0) {
    int secondQuote = cmtData.indexOf('"', firstQuote + 1);
    if (secondQuote > firstQuote) {
      return cmtData.substring(firstQuote + 1, secondQuote);
    }
  }
  return "";
}

// 提取CMT PDU数据
String extractCMTPDU(const String& cmtData) {
  int newlinePos = cmtData.indexOf('\n');
  if (newlinePos >= 0) {
    String pduLine = cmtData.substring(newlinePos + 1);
    pduLine.trim();
    
    logManager.addLog(LOG_DEBUG, "CMT_PDU", "原始PDU: " + pduLine.substring(0, 50) + "...");
    
    // 直接返回完整PDU数据，让解码函数处理
    return pduLine;
  }
  return "";
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

// 处理CMT格式短信
void handleCMTSMS(const String& cmtData) {
  String sender = extractCMTSender(cmtData);
  String pduData = extractCMTPDU(cmtData);
  
  if (sender.isEmpty() || pduData.isEmpty()) {
    logManager.addLog(LOG_WARN, "SMS_CMT", "无法解析CMT数据");
    return;
  }
  
  // 解析UDH和长短信
  uint16_t ref;
  uint8_t total, seq;
  String payloadHex;
  bool hasConcat = parseUdhAndExtractPayload(pduData, ref, total, seq, payloadHex);
  
  if (hasConcat) {
    logManager.addLog(LOG_INFO, "SMS_CMT", "长短信分片: " + sender + " ref=" + String(ref) + " " + String(seq) + "/" + String(total));
    
    String assembled = storeSegmentAndAssemble(sender, ref, total, seq, payloadHex);
    if (!assembled.isEmpty()) {
      String content = decodeUCS2BE(assembled);
      String timestamp = String(millis());
      smsStorage.saveSMS(sender, content, timestamp, true);
      logManager.addLog(LOG_INFO, "SMS", "收到长短信，时间" + timestamp + ",发件人" + sender + "， 内容" + content);
    }
  } else {
    String content = decodeUCS2BE(payloadHex);
    String timestamp = String(millis());
    smsStorage.saveSMS(sender, content, timestamp, true);
    logManager.addLog(LOG_INFO, "SMS", "收到短信，时间" + timestamp + ",发件人" + sender + "， 内容" + content);
  }
}

// CMT短信缓存
std::vector<String> pendingCMTMessages;

// 存储待处理的CMT短信
void storePendingCMTSMS(const String& cmtData) {
  pendingCMTMessages.push_back(cmtData);
  logManager.addLog(LOG_DEBUG, "SMS_CMT", "存储CMT短信，待处理数量: " + String(pendingCMTMessages.size()));
}

// 处理所有待处理的CMT短信
void processPendingCMTSMS() {
  logManager.addLog(LOG_INFO, "SMS_CMT", "开始处理 " + String(pendingCMTMessages.size()) + " 条CMT短信");
  
  for (const String& cmtData : pendingCMTMessages) {
    handleCMTSMS(cmtData);
  }
  
  pendingCMTMessages.clear();
}



// 清空临时存储
void clearTempSMSStorage() {
  SPIFFS.remove("/temp_sms.txt");
}

// 处理所有批量读取的短信
void processBatchedSMS() {
  logManager.addLog(LOG_INFO, "SMS_BATCH", "开始处理批量短信");
  
  File file = SPIFFS.open("/temp_sms.txt", "r");
  if (!file) {
    logManager.addLog(LOG_WARN, "SMS_BATCH", "临时文件不存在");
    return;
  }
  
  // 检查文件大小
  size_t fileSize = file.size();
  logManager.addLog(LOG_INFO, "SMS_BATCH", "临时文件大小: " + String(fileSize) + " 字节");
  
  // 分两阶段：1.处理长短信 2.处理普通短信
  processLongSMSFromTemp(file);
  file.seek(0);
  processNormalSMSFromTemp(file);
  
  file.close();
  SPIFFS.remove("/temp_sms.txt");
  
  logManager.addLog(LOG_INFO, "SMS_BATCH", "批量短信处理完成");
}