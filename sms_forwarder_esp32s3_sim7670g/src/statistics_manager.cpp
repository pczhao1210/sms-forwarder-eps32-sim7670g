#include "statistics_manager.h"
#include "log_manager.h"

StatisticsManager statisticsManager;
Statistics StatisticsManager::stats = {0};
unsigned long StatisticsManager::startTime = 0;

void StatisticsManager::incrementSMSReceived() {
  stats.totalSMSReceived++;
}

void StatisticsManager::incrementSMSForwarded() {
  stats.totalSMSForwarded++;
}

void StatisticsManager::incrementSMSFiltered() {
  stats.totalSMSFiltered++;
}

void StatisticsManager::incrementPushSuccess() {
  stats.totalPushSuccess++;
}

void StatisticsManager::incrementPushFailed() {
  stats.totalPushFailed++;
}

void StatisticsManager::incrementRetries() {
  stats.totalRetries++;
}

void StatisticsManager::updateLastSMS(const String& sender) {
  stats.lastSender = sender;
  stats.lastSMSTime = String(millis());
}

Statistics StatisticsManager::getStatistics() {
  stats.uptime = (millis() - startTime) / 1000;
  return stats;
}

void StatisticsManager::resetStatistics() {
  stats = {0};
  startTime = millis();
  logManager.addLog(LOG_INFO, "STATS", "统计数据已重置");
}

void StatisticsManager::saveStatistics() {
  // 简化实现，实际可保存到文件
}

void StatisticsManager::loadStatistics() {
  startTime = millis();
}