#include "retry_manager.h"
#include "notification_manager.h"
#include "statistics_manager.h"
#include "log_manager.h"

RetryManager retryManager;

void RetryManager::scheduleRetry(const String& sender, const String& content) {
  for (const auto& task : retryQueue) {
    if (task.sender == sender && task.content == content) {
      LOGI("RETRY", "retry_task_exists", sender.c_str());
      return;
    }
  }
  
  RetryTask task = {sender, content, 0, millis() + RETRY_INTERVAL};
  retryQueue.push_back(task);
  
  LOGI("RETRY", "retry_scheduled", sender.c_str());
}

void RetryManager::processRetries() {
  unsigned long now = millis();
  
  for (auto it = retryQueue.begin(); it != retryQueue.end();) {
    if (now >= it->nextRetry) {
      statisticsManager.incrementRetries();
      
      bool success = notificationManager.forwardSMS(it->sender, it->content, true);
      it->retryCount++;
      
      if (success) {
        LOGI("RETRY", "retry_success");
        it = retryQueue.erase(it);
      } else if (it->retryCount >= MAX_RETRY_COUNT) {
        LOGE("RETRY", "retry_give_up");
        it = retryQueue.erase(it);
      } else {
        it->nextRetry = now + (RETRY_INTERVAL * (it->retryCount + 1)); // 递增延迟
        LOGI("RETRY", "retry_reschedule", String(it->retryCount + 1).c_str());
        ++it;
      }
    } else {
      ++it;
    }
  }
}

void RetryManager::clearRetries() {
  retryQueue.clear();
  LOGI("RETRY", "retry_cleared");
}

int RetryManager::getRetryCount() {
  return retryQueue.size();
}
