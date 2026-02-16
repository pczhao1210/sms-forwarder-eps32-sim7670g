#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <vector>
#include <Arduino.h>

#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3

struct LogEntry {
  unsigned long timestamp;
  uint8_t level;
  String tag;
  String message;
};

class LogManager {
private:
  static const size_t MAX_LOG_ENTRIES = 1000;
  std::vector<LogEntry> logBuffer;
  
public:
  void addLog(uint8_t level, const String& tag, const String& message);
  void addLogf(uint8_t level, const String& tag, const char* key, ...);
  void addInitLog(const String& module, bool success);
  String getLogsAsJson(uint8_t minLevel = 0, const String& filter = "");
  void clearLogs();
  void trimLogs(size_t maxEntries);
  size_t getLogCount();
  String escapeJson(const String& str);
};

extern LogManager logManager;

// 统一的日志宏
#define LOG_SERIAL(msg) logManager.addLog(LOG_INFO, "SERIAL", msg)
#define LOG_INIT(module, success) logManager.addInitLog(module, success)
#define LOGD(tag, key, ...) logManager.addLogf(LOG_DEBUG, tag, key, ##__VA_ARGS__)
#define LOGI(tag, key, ...) logManager.addLogf(LOG_INFO, tag, key, ##__VA_ARGS__)
#define LOGW(tag, key, ...) logManager.addLogf(LOG_WARN, tag, key, ##__VA_ARGS__)
#define LOGE(tag, key, ...) logManager.addLogf(LOG_ERROR, tag, key, ##__VA_ARGS__)

#endif
