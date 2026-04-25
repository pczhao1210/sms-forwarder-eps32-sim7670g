#include "retry_manager.h"

#include "log_manager.h"
#include "notification_manager.h"
#include "sms_storage.h"
#include "statistics_manager.h"
#include "time_manager.h"

RetryManager retryManager;

void RetryManager::scheduleRetry(int smsId, const String& sender, const String& content) {
  if (smsId <= 0) {
    LOGW("RETRY", "retry_skip_no_sms_id");
    return;
  }

  for (const auto& task : retryQueue) {
    if (task.smsId == smsId && task.sender == sender && task.content == content) {
      LOGI("RETRY", "retry_task_exists", sender.c_str());
      return;
    }
  }

  RetryTask task = {smsId, sender, content, 0, millis() + RETRY_INTERVAL, false};
  retryQueue.push_back(task);

  if (smsId > 0) {
    smsStorage.updateSMSStatus(smsId, SMSStatus::RETRY_SCHEDULED, getTimestampMsString(), "", 0);
  }

  LOGI("RETRY", "retry_scheduled", sender.c_str());
}

void RetryManager::processRetries() {
  unsigned long now = millis();

  for (auto it = retryQueue.begin(); it != retryQueue.end();) {
    if (it->inFlight || now < it->nextRetry) {
      ++it;
      continue;
    }

    int nextAttempt = it->retryCount + 1;
    if (!notificationManager.forwardSMS(it->sender, it->content, true, it->smsId, false)) {
      it->nextRetry = now + 5000UL;
      ++it;
      continue;
    }

    statisticsManager.incrementRetries();
    it->retryCount = nextAttempt;
    it->inFlight = true;
    if (it->smsId > 0) {
      smsStorage.updateSMSStatus(it->smsId, SMSStatus::RETRYING, getTimestampMsString(), "", nextAttempt);
    }
    ++it;
  }
}

void RetryManager::handleRetryResult(int smsId, const String& sender, const String& content, bool success) {
  unsigned long now = millis();

  for (auto it = retryQueue.begin(); it != retryQueue.end(); ++it) {
    if (it->smsId != smsId || it->sender != sender || it->content != content) {
      continue;
    }

    if (success) {
      if (it->smsId > 0) {
        smsStorage.updateSMSStatus(it->smsId, SMSStatus::FORWARD_SUCCESS, getTimestampMsString(), "", it->retryCount);
      }
      statisticsManager.incrementSMSForwarded();
      LOGI("RETRY", "retry_success");
      retryQueue.erase(it);
      return;
    }

    if (it->retryCount >= MAX_RETRY_COUNT) {
      if (it->smsId > 0) {
        smsStorage.updateSMSStatus(it->smsId, SMSStatus::RETRY_EXHAUSTED, getTimestampMsString(), "retry_exhausted", it->retryCount);
      }
      LOGE("RETRY", "retry_give_up");
      retryQueue.erase(it);
      return;
    }

    if (it->smsId > 0) {
      smsStorage.updateSMSStatus(it->smsId, SMSStatus::RETRY_SCHEDULED, getTimestampMsString(), "", it->retryCount);
    }
    it->inFlight = false;
    it->nextRetry = now + (RETRY_INTERVAL * (it->retryCount + 1));
    LOGI("RETRY", "retry_reschedule", String(it->retryCount + 1).c_str());
    return;
  }
}

void RetryManager::clearRetries() {
  retryQueue.clear();
  LOGI("RETRY", "retry_cleared");
}

void RetryManager::cancelRetry(int smsId) {
  if (smsId <= 0) return;
  for (auto it = retryQueue.begin(); it != retryQueue.end();) {
    if (it->smsId == smsId) {
      it = retryQueue.erase(it);
    } else {
      ++it;
    }
  }
}

int RetryManager::getRetryCount() {
  return static_cast<int>(retryQueue.size());
}
