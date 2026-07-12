#include "retry_manager.h"

#include "log_manager.h"
#include "millis_utils.h"
#include "notification_manager.h"
#include "sms_storage.h"
#include "statistics_manager.h"
#include "time_manager.h"

RetryManager retryManager;

static bool shouldRestoreRetryStatus(const String& status) {
  return status == SMSStatus::RETRY_SCHEDULED ||
         status == SMSStatus::RETRYING ||
         status == SMSStatus::PENDING_FORWARD;
}

void RetryManager::restoreRetriesFromStorage() {
  int restored = 0;
  bool statusesChanged = false;
  int total = smsStorage.getSMSCount();
  for (int index = 0; index < total; index++) {
    SMSRecord record;
    if (!smsStorage.getSMSAt(static_cast<size_t>(index), record)) continue;
    if (!shouldRestoreRetryStatus(record.status)) continue;

    bool exists = false;
    for (const auto& task : retryQueue) {
      if (task.smsId == record.id) {
        exists = true;
        break;
      }
    }
    if (exists) continue;

    int retryCount = record.retryCount < 0 ? 0 : record.retryCount;
    RetryTask task = {
        record.id,
        record.sender,
        record.content,
        retryCount,
        millisDeadlineAfter(millis(), 10000UL),
        false};
    retryQueue.push_back(task);

    if (record.status != SMSStatus::RETRY_SCHEDULED) {
      if (smsStorage.updateSMSStatus(record.id, SMSStatus::RETRY_SCHEDULED, "", record.lastError, retryCount, false)) {
        statusesChanged = true;
      }
    }
    restored++;
  }

  if (restored > 0) {
    logManager.addLog(LOG_INFO, "RETRY", "Restored retry tasks: " + String(restored));
  }
  if (statusesChanged && !smsStorage.flush()) {
    logManager.addLog(LOG_ERROR, "RETRY", "Failed to persist restored retry states");
  }
}

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

  RetryTask task = {
      smsId,
      sender,
      content,
      0,
      millisDeadlineAfter(millis(), RETRY_INTERVAL),
      false};
  retryQueue.push_back(task);

  LOGI("RETRY", "retry_scheduled", sender.c_str());
}

void RetryManager::processRetries() {
  unsigned long now = millis();

  for (auto it = retryQueue.begin(); it != retryQueue.end();) {
    if (it->inFlight || !millisDeadlineReached(now, it->nextRetry)) {
      ++it;
      continue;
    }

    int nextAttempt = it->retryCount + 1;
    if (!notificationManager.forwardSMS(it->sender, it->content, true, it->smsId, false)) {
      it->nextRetry = millisDeadlineAfter(now, 5000UL);
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
    it->nextRetry =
        millisDeadlineAfter(now, RETRY_INTERVAL * (it->retryCount + 1));
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
