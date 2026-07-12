#include "statistics_manager.h"
#include "log_manager.h"
#include "time_manager.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>

StatisticsManager statisticsManager;
Statistics StatisticsManager::stats = {0};
unsigned long StatisticsManager::startTime = 0;

static const char* STATS_FILE_PATH = "/statistics.json";
static const char* STATS_TMP_FILE_PATH = "/statistics.tmp";

static void populateStatsDocument(JsonDocument& doc, const Statistics& stats) {
  doc["totalSMSReceived"] = stats.totalSMSReceived;
  doc["totalSMSForwarded"] = stats.totalSMSForwarded;
  doc["totalSMSFiltered"] = stats.totalSMSFiltered;
  doc["totalPushSuccess"] = stats.totalPushSuccess;
  doc["totalPushFailed"] = stats.totalPushFailed;
  doc["totalRetries"] = stats.totalRetries;
  doc["lastSMSTime"] = stats.lastSMSTime;
  doc["lastSender"] = stats.lastSender;
}

void StatisticsManager::incrementSMSReceived() {
  stats.totalSMSReceived++;
  saveStatistics();
}

void StatisticsManager::incrementSMSForwarded() {
  stats.totalSMSForwarded++;
  saveStatistics();
}

void StatisticsManager::incrementSMSFiltered() {
  stats.totalSMSFiltered++;
  saveStatistics();
}

void StatisticsManager::incrementPushSuccess() {
  stats.totalPushSuccess++;
  saveStatistics();
}

void StatisticsManager::incrementPushFailed() {
  stats.totalPushFailed++;
  saveStatistics();
}

void StatisticsManager::incrementRetries() {
  stats.totalRetries++;
  saveStatistics();
}

void StatisticsManager::updateLastSMS(const String& sender) {
  stats.lastSender = sender;
  stats.lastSMSTime = getTimestampMsString();
  saveStatistics();
}

Statistics StatisticsManager::getStatistics() {
  stats.uptime = (millis() - startTime) / 1000;
  return stats;
}

void StatisticsManager::resetStatistics() {
  stats = {0};
  startTime = millis();
  saveStatistics();
  LOGI("STATS", "stats_reset");
}

void StatisticsManager::saveStatistics() {
  DynamicJsonDocument doc(512);
  populateStatsDocument(doc, stats);

  SPIFFS.remove(STATS_TMP_FILE_PATH);
  File file = SPIFFS.open(STATS_TMP_FILE_PATH, "w");
  if (!file) {
    LOGE("STATS", "stats_save_fail", "open");
    return;
  }

  size_t bytesWritten = serializeJson(doc, file);
  file.flush();
  file.close();
  if (bytesWritten == 0) {
    SPIFFS.remove(STATS_TMP_FILE_PATH);
    LOGE("STATS", "stats_save_fail", "write");
    return;
  }

  SPIFFS.remove(STATS_FILE_PATH);
  if (!SPIFFS.rename(STATS_TMP_FILE_PATH, STATS_FILE_PATH)) {
    SPIFFS.remove(STATS_TMP_FILE_PATH);
    LOGE("STATS", "stats_save_fail", "rename");
  }
}

void StatisticsManager::loadStatistics() {
  stats = {0};
  startTime = millis();

  File file = SPIFFS.open(STATS_FILE_PATH, "r");
  if (!file) {
    return;
  }

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    LOGW("STATS", "stats_load_fail", error.c_str());
    return;
  }

  stats.totalSMSReceived = doc["totalSMSReceived"] | 0;
  stats.totalSMSForwarded = doc["totalSMSForwarded"] | 0;
  stats.totalSMSFiltered = doc["totalSMSFiltered"] | 0;
  stats.totalPushSuccess = doc["totalPushSuccess"] | 0;
  stats.totalPushFailed = doc["totalPushFailed"] | 0;
  stats.totalRetries = doc["totalRetries"] | 0;
  stats.lastSMSTime = doc["lastSMSTime"].as<String>();
  stats.lastSender = doc["lastSender"].as<String>();
}
