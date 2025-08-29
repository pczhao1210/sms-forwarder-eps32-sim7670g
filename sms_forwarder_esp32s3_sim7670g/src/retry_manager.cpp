#include "retry_manager.h"
#include "notification_manager.h"
#include "statistics_manager.h"
#include "log_manager.h"

RetryManager retryManager;

void RetryManager::scheduleRetry(const String& sender, const String& content) {
  RetryTask task = {sender, content, 0, millis() + RETRY_INTERVAL};
  retryQueue.push_back(task);
  
  logManager.addLog(LOG_INFO, "RETRY", "安排重试推送: " + sender);
}

void RetryManager::processRetries() {
  unsigned long now = millis();
  
  for (auto it = retryQueue.begin(); it != retryQueue.end();) {
    if (now >= it->nextRetry) {
      if (it->retryCount < MAX_RETRY_COUNT) {
        // 重试推送
        notificationManager.forwardSMS(it->sender, it->content);
        
        it->retryCount++;
        it->nextRetry = now + (RETRY_INTERVAL * (it->retryCount + 1)); // 递增延迟
        
        statisticsManager.incrementRetries();
        logManager.addLog(LOG_INFO, "RETRY", 
          "重试推送 " + String(it->retryCount) + "/" + String(MAX_RETRY_COUNT));
        
        ++it;
      } else {
        logManager.addLog(LOG_ERROR, "RETRY", "重试次数超限，放弃推送");
        it = retryQueue.erase(it);
      }
    } else {
      ++it;
    }
  }
}

void RetryManager::clearRetries() {
  retryQueue.clear();
  logManager.addLog(LOG_INFO, "RETRY", "清空重试队列");
}

int RetryManager::getRetryCount() {
  return retryQueue.size();
}