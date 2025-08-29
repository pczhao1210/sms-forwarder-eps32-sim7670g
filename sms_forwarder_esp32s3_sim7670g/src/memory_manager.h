#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <cstddef>

class MemoryManager {
public:
  static void optimizeMemory();
  static void emergencyCleanup();
  static size_t getFreeHeap();
  static float getHeapUsagePercent();
  
private:
  static const size_t CRITICAL_HEAP_SIZE = 50000; // 50KB临界值
};

extern MemoryManager memoryManager;

#endif