#include "log_manager.h"
#include "config_manager.h"
#include "i18n.h"
#include "time_manager.h"
#include <stdarg.h>

LogManager logManager;

static uint64_t resolveLogTimestampMs(unsigned long entryUptimeMs, bool timeReady, uint64_t epochOffsetMs) {
  if (!timeReady) {
    return static_cast<uint64_t>(entryUptimeMs);
  }
  return epochOffsetMs + static_cast<uint64_t>(entryUptimeMs);
}

void LogManager::addLog(uint8_t level, const String& tag, const String& message) {
  LogEntry entry = {millis(), level, tag, message};
  
  // 循环缓冲区管理
  if (logBuffer.size() >= MAX_LOG_ENTRIES) {
    logBuffer.erase(logBuffer.begin());
  }
  logBuffer.push_back(entry);
  
  // 使用config.debug.atCommandEcho控制Serial输出
  if (config.debug.atCommandEcho) {
    String levelStr[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    Serial.printf("[%s] %s: %s\n", levelStr[level].c_str(), tag.c_str(), message.c_str());
  }
}

void LogManager::addLogf(uint8_t level, const String& tag, const char* key, ...) {
  const char* format = i18nGet(key);
  char buffer[256];
  va_list args;
  va_start(args, key);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  addLog(level, tag, String(buffer));
}

void LogManager::addInitLog(const String& module, bool success) {
  String message = i18nFormat(success ? "init_module" : "init_module_fail", module.c_str());
  addLog(LOG_INFO, "INIT", message);
  
  // 初始化阶段的特殊格式输出，使用config控制
  if (config.debug.atCommandEcho) {
    Serial.print("[INIT] ");
    Serial.println(message);
  }
}

String LogManager::escapeJson(const String& str) {
  String escaped = "";
  escaped.reserve(str.length() * 2); // 预分配空间
  
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    switch (c) {
      case '\\': escaped += "\\\\"; break;
      case '"': escaped += "\\\""; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      case '\b': escaped += "\\b"; break;
      case '\f': escaped += "\\f"; break;
      default:
        if (c < 32) {
          // 跳过控制字符
        } else {
          escaped += c;
        }
        break;
    }
  }
  return escaped;
}

String LogManager::getLogsAsJson(uint8_t minLevel, const String& filter) {
  String result = "{\"logs\":[";
  bool first = true;
  int count = 0;
  const int MAX_LOGS_RETURN = 100; // 限制返回日志数量
  uint64_t nowEpochMs = getEpochMillis();
  unsigned long nowUptimeMs = millis();
  bool timeReady = (nowEpochMs > 0 && nowEpochMs > static_cast<uint64_t>(nowUptimeMs));
  uint64_t epochOffsetMs = timeReady ? (nowEpochMs - static_cast<uint64_t>(nowUptimeMs)) : 0;
  
  // 从后往前挑选最新的日志，再按正序输出
  std::vector<int> matched;
  matched.reserve(MAX_LOGS_RETURN);
  for (int i = (int)logBuffer.size() - 1; i >= 0 && (int)matched.size() < MAX_LOGS_RETURN; i--) {
    const auto& entry = logBuffer[i];
    if (entry.level >= minLevel && (filter.isEmpty() || entry.message.indexOf(filter) >= 0)) {
      matched.push_back(i);
    }
  }
  
  for (int i = (int)matched.size() - 1; i >= 0; i--) {
    const auto& entry = logBuffer[matched[i]];
    if (!first) result += ",";
    
    // 限制字段长度防止JSON过大
    String tag = entry.tag.substring(0, 20);
    String message = entry.message.substring(0, 200);
    
    uint64_t resolvedTs = resolveLogTimestampMs(entry.timestamp, timeReady, epochOffsetMs);
    char tsBuf[24];
    snprintf(tsBuf, sizeof(tsBuf), "%llu", static_cast<unsigned long long>(resolvedTs));

    result += "{\"timestamp\":" + String(tsBuf);
    result += ",\"level\":" + String(entry.level);
    result += ",\"tag\":\"" + escapeJson(tag) + "\"";
    result += ",\"message\":\"" + escapeJson(message) + "\"}";
    first = false;
    count++;
  }
  
  result += "],\"total\":" + String(logBuffer.size()) + ",\"returned\":" + String(count) + "}";
  
  // 验证JSON格式
  if (result.length() > 50000) {
    // JSON过大，返回简化版本
    return "{\"logs\":[],\"error\":\"日志数据过大，请清空日志\",\"total\":" + String(logBuffer.size()) + "}";
  }
  
  return result;
}

void LogManager::clearLogs() {
  logBuffer.clear();
  addLog(LOG_INFO, "LOG_MGR", i18nGet("log_cleared"));
}

void LogManager::trimLogs(size_t maxEntries) {
  if (logBuffer.size() > maxEntries) {
    logBuffer.erase(logBuffer.begin(), logBuffer.begin() + (logBuffer.size() - maxEntries));
    addLog(LOG_INFO, "LOG_MGR", i18nGet("log_trimmed"));
  }
}

size_t LogManager::getLogCount() {
  return logBuffer.size();
}
