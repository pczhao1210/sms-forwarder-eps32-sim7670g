#include "memory_manager.h"
#include <Arduino.h>

MemoryManager memoryManager;

void MemoryManager::optimizeMemory() {
  // 简化内存优化，只做基本检查
  if (ESP.getFreeHeap() < CRITICAL_HEAP_SIZE) {
    emergencyCleanup();
  }
}

void MemoryManager::emergencyCleanup() {
  // 简化紧急清理
  Serial.println("内存不足，执行紧急清理");
}

size_t MemoryManager::getFreeHeap() {
  return ESP.getFreeHeap();
}

float MemoryManager::getHeapUsagePercent() {
  size_t totalHeap = ESP.getHeapSize();
  size_t freeHeap = ESP.getFreeHeap();
  return ((float)(totalHeap - freeHeap) / totalHeap) * 100.0;
}