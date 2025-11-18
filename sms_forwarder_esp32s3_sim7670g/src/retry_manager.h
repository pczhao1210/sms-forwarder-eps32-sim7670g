#ifndef RETRY_MANAGER_H
#define RETRY_MANAGER_H

#include <vector>
#include <Arduino.h>

struct RetryTask {
  String sender;
  String content;
  int retryCount;
  unsigned long nextRetry;
};

class RetryManager {
private:
  std::vector<RetryTask> retryQueue;
  static const int MAX_RETRY_COUNT = 3;
  static const unsigned long RETRY_INTERVAL = 60000; // 1分钟
  
public:
  void scheduleRetry(const String& sender, const String& content);
  void processRetries();
  void clearRetries();
  int getRetryCount();
};

extern RetryManager retryManager;

#endif
