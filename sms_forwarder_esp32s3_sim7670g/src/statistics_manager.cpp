#include "statistics_manager.h"
#include "log_manager.h"
#include "millis_utils.h"
#include "time_manager.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <esp_timer.h>

StatisticsManager statisticsManager;
Statistics StatisticsManager::stats = {0};

static const char* STATS_FILE_PATH = "/statistics.json";
static const char* STATS_TMP_FILE_PATH = "/statistics.tmp";
static const char* STATS_BACKUP_FILE_PATH = "/statistics.bak";
static const char* STATS_CORRUPT_FILE_PATH = "/statistics.corrupt";
static const unsigned long STATS_DIRTY_FLUSH_INTERVAL_MS = 30000UL;
static const unsigned long STATS_UPTIME_CHECKPOINT_INTERVAL_MS = 60UL * 60UL * 1000UL;
static uint64_t persistedUptimeSeconds = 0;
static int64_t bootUptimeStartMicros = 0;
static bool statisticsDirty = false;
static unsigned long lastStatisticsSaveMs = 0;

static void refreshUptime(Statistics& stats) {
  int64_t nowMicros = esp_timer_get_time();
  uint64_t elapsedSeconds = nowMicros > bootUptimeStartMicros
                                ? static_cast<uint64_t>(nowMicros - bootUptimeStartMicros) / 1000000ULL
                                : 0;
  uint64_t totalSeconds = persistedUptimeSeconds + elapsedSeconds;
  stats.uptime = totalSeconds > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : static_cast<uint32_t>(totalSeconds);
}

static void markStatisticsDirty() {
  statisticsDirty = true;
}

static void populateStatsDocument(JsonDocument& doc, const Statistics& stats) {
  doc["totalSMSReceived"] = stats.totalSMSReceived;
  doc["totalSMSForwarded"] = stats.totalSMSForwarded;
  doc["totalSMSFiltered"] = stats.totalSMSFiltered;
  doc["totalPushSuccess"] = stats.totalPushSuccess;
  doc["totalPushFailed"] = stats.totalPushFailed;
  doc["totalRetries"] = stats.totalRetries;
  doc["uptime"] = stats.uptime;
  doc["lastDailyReportDate"] = stats.lastDailyReportDate;
  doc["lastWeeklyReportDate"] = stats.lastWeeklyReportDate;
  doc["lastSMSTime"] = stats.lastSMSTime;
  doc["lastSender"] = stats.lastSender;
}

static bool loadStatsDocument(const char* path, DynamicJsonDocument& doc, String& errorOut) {
  File file = SPIFFS.open(path, "r");
  if (!file) {
    errorOut = "open";
    return false;
  }

  doc.clear();
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    errorOut = error.c_str();
    return false;
  }
  if (!doc.is<JsonObject>()) {
    errorOut = "invalid_root";
    return false;
  }
  return true;
}

void StatisticsManager::incrementSMSReceived() {
  stats.totalSMSReceived++;
  markStatisticsDirty();
}

void StatisticsManager::incrementSMSForwarded() {
  stats.totalSMSForwarded++;
  markStatisticsDirty();
}

void StatisticsManager::incrementSMSFiltered() {
  stats.totalSMSFiltered++;
  markStatisticsDirty();
}

void StatisticsManager::incrementPushSuccess() {
  stats.totalPushSuccess++;
  markStatisticsDirty();
}

void StatisticsManager::incrementPushFailed() {
  stats.totalPushFailed++;
  markStatisticsDirty();
}

void StatisticsManager::incrementRetries() {
  stats.totalRetries++;
  markStatisticsDirty();
}

void StatisticsManager::updateLastSMS(const String& sender) {
  stats.lastSender = sender;
  stats.lastSMSTime = getTimestampMsString();
  markStatisticsDirty();
}

Statistics StatisticsManager::getStatistics() {
  refreshUptime(stats);
  return stats;
}

bool StatisticsManager::wasDailyReportSent(int32_t dateKey) {
  return dateKey > 0 && stats.lastDailyReportDate == dateKey;
}

bool StatisticsManager::wasWeeklyReportSent(int32_t dateKey) {
  return dateKey > 0 && stats.lastWeeklyReportDate == dateKey;
}

void StatisticsManager::markDailyReportSent(int32_t dateKey) {
  stats.lastDailyReportDate = dateKey;
  statisticsDirty = true;
  saveStatistics();
}

void StatisticsManager::markWeeklyReportSent(int32_t dateKey) {
  stats.lastWeeklyReportDate = dateKey;
  statisticsDirty = true;
  saveStatistics();
}

void StatisticsManager::resetStatistics() {
  int32_t lastDailyReportDate = stats.lastDailyReportDate;
  int32_t lastWeeklyReportDate = stats.lastWeeklyReportDate;
  stats = {0};
  stats.lastDailyReportDate = lastDailyReportDate;
  stats.lastWeeklyReportDate = lastWeeklyReportDate;
  persistedUptimeSeconds = 0;
  bootUptimeStartMicros = esp_timer_get_time();
  statisticsDirty = true;
  saveStatistics();
  LOGI("STATS", "stats_reset");
}

void StatisticsManager::saveStatistics() {
  refreshUptime(stats);
  DynamicJsonDocument doc(512);
  populateStatsDocument(doc, stats);

  SPIFFS.remove(STATS_TMP_FILE_PATH);
  File file = SPIFFS.open(STATS_TMP_FILE_PATH, "w");
  if (!file) {
    statisticsDirty = true;
    LOGE("STATS", "stats_save_fail", "open");
    return;
  }

  size_t bytesWritten = serializeJson(doc, file);
  file.flush();
  file.close();
  if (bytesWritten == 0) {
    SPIFFS.remove(STATS_TMP_FILE_PATH);
    LOGE("STATS", "stats_save_fail", "write");
    statisticsDirty = true;
    return;
  }

  bool hadExisting = SPIFFS.exists(STATS_FILE_PATH);
  SPIFFS.remove(STATS_BACKUP_FILE_PATH);
  if (hadExisting && !SPIFFS.rename(STATS_FILE_PATH, STATS_BACKUP_FILE_PATH)) {
    SPIFFS.remove(STATS_TMP_FILE_PATH);
    LOGE("STATS", "stats_save_fail", "backup");
    statisticsDirty = true;
    return;
  }

  if (!SPIFFS.rename(STATS_TMP_FILE_PATH, STATS_FILE_PATH)) {
    if (hadExisting) {
      SPIFFS.rename(STATS_BACKUP_FILE_PATH, STATS_FILE_PATH);
    }
    SPIFFS.remove(STATS_TMP_FILE_PATH);
    LOGE("STATS", "stats_save_fail", "rename");
    statisticsDirty = true;
    return;
  }

  persistedUptimeSeconds = stats.uptime;
  bootUptimeStartMicros = esp_timer_get_time();
  statisticsDirty = false;
  lastStatisticsSaveMs = millis();
}

void StatisticsManager::processPersistence() {
  unsigned long now = millis();
  bool dirtyDue = statisticsDirty &&
                  millisElapsed(now, lastStatisticsSaveMs, STATS_DIRTY_FLUSH_INTERVAL_MS);
  bool checkpointDue = millisElapsed(now, lastStatisticsSaveMs, STATS_UPTIME_CHECKPOINT_INTERVAL_MS);
  if (dirtyDue || checkpointDue) {
    saveStatistics();
  }
}

void StatisticsManager::loadStatistics() {
  stats = {0};
  persistedUptimeSeconds = 0;
  bootUptimeStartMicros = esp_timer_get_time();
  statisticsDirty = false;
  lastStatisticsSaveMs = millis();

  DynamicJsonDocument doc(512);
  const char* loadPaths[] = {
    STATS_FILE_PATH,
    STATS_BACKUP_FILE_PATH,
    STATS_TMP_FILE_PATH
  };
  const char* loadedPath = nullptr;
  String loadError;
  for (const char* path : loadPaths) {
    if (!SPIFFS.exists(path)) continue;
    if (loadStatsDocument(path, doc, loadError)) {
      loadedPath = path;
      break;
    }
    LOGW("STATS", "stats_load_fail", loadError.c_str());
  }

  if (!loadedPath) {
    return;
  }

  if (strcmp(loadedPath, STATS_FILE_PATH) != 0) {
    if (SPIFFS.exists(STATS_FILE_PATH)) {
      SPIFFS.remove(STATS_CORRUPT_FILE_PATH);
      SPIFFS.rename(STATS_FILE_PATH, STATS_CORRUPT_FILE_PATH);
    }
    if (!SPIFFS.exists(STATS_FILE_PATH) && !SPIFFS.rename(loadedPath, STATS_FILE_PATH)) {
      LOGW("STATS", "stats_load_fail", "recover_rename");
    }
  }

  stats.totalSMSReceived = doc["totalSMSReceived"] | 0;
  stats.totalSMSForwarded = doc["totalSMSForwarded"] | 0;
  stats.totalSMSFiltered = doc["totalSMSFiltered"] | 0;
  stats.totalPushSuccess = doc["totalPushSuccess"] | 0;
  stats.totalPushFailed = doc["totalPushFailed"] | 0;
  stats.totalRetries = doc["totalRetries"] | 0;
  stats.uptime = doc["uptime"] | 0;
  stats.lastDailyReportDate = doc["lastDailyReportDate"] | 0;
  stats.lastWeeklyReportDate = doc["lastWeeklyReportDate"] | 0;
  persistedUptimeSeconds = stats.uptime;
  bootUptimeStartMicros = esp_timer_get_time();
  stats.lastSMSTime = doc["lastSMSTime"].as<String>();
  stats.lastSender = doc["lastSender"].as<String>();
}
