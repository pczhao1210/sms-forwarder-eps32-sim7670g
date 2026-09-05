#ifndef RETRY_MANAGER_H
#define RETRY_MANAGER_H

#include <vector>
#include <Arduino.h>

struct RetryTask {
  int smsId;
  String sender;
  String content;
  int retryCount;
  unsigned long nextRetry;
  bool inFlight;
};

class RetryManager {
private:
  std::vector<RetryTask> retryQueue;
  static const int MAX_RETRY_COUNT = 3;
  static const unsigned long RETRY_INTERVAL = 60000; // 1分钟
  
public:
  void restoreRetriesFromStorage();
  void scheduleRetry(int smsId, const String& sender, const String& content);
  void processRetries();
  bool handleRetryResult(int smsId, const String& sender, const String& content, bool success);
  void clearRetries();
  void cancelRetry(int smsId);
  int getRetryCount();
};

extern RetryManager retryManager;

#endif
