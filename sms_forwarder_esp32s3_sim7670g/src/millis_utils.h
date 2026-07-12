#ifndef MILLIS_UTILS_H
#define MILLIS_UTILS_H

#include <stdint.h>

inline uint32_t millisSince(uint32_t now, uint32_t start) {
  return static_cast<uint32_t>(now - start);
}

inline bool millisElapsed(uint32_t now, uint32_t start, uint32_t interval) {
  return millisSince(now, start) >= interval;
}

inline bool millisDeadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

#endif
