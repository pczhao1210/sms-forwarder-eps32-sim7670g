#include "retry_manager.h"
#include "notification_manager.h"
#include "statistics_manager.h"
#include "log_manager.h"

RetryManager retryManager;

void RetryManager::scheduleRetry(const String& sender, const String& content) {
  for (const auto& task : retryQueue) {
    if (task.sender == sender && task.content == content) {
      logManager.addLog(LOG_INFO, "RETRY", "已存在相同任务，跳过重复入队: " + sender);
      return;
    }
  }
  
  RetryTask task = {sender, content, 0, millis() + RETRY_INTERVAL};
  retryQueue.push_back(task);
  
  logManager.addLog(LOG_INFO, "RETRY", "安排重试推送: " + sender);
}

void RetryManager::processRetries() {
  unsigned long now = millis();
  
  for (auto it = retryQueue.begin(); it != retryQueue.end();) {
    if (now >= it->nextRetry) {
      statisticsManager.incrementRetries();
      
      bool success = notificationManager.forwardSMS(it->sender, it->content, true);
      it->retryCount++;
      
      if (success) {
        logManager.addLog(LOG_INFO, "RETRY", "重试成功，移除任务");
        it = retryQueue.erase(it);
      } else if (it->retryCount >= MAX_RETRY_COUNT) {
        logManager.addLog(LOG_ERROR, "RETRY", "重试次数超限，放弃推送");
        it = retryQueue.erase(it);
      } else {
        it->nextRetry = now + (RETRY_INTERVAL * (it->retryCount + 1)); // 递增延迟
        logManager.addLog(LOG_INFO, "RETRY", 
          "重试失败，重新安排第 " + String(it->retryCount + 1) + " 次尝试");
        ++it;
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
