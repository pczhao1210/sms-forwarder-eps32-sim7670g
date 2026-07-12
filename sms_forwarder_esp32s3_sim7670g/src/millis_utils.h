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
  // Signed subtraction keeps deadline ordering valid across one 32-bit wrap.
  return static_cast<int32_t>(now - deadline) >= 0;
}

inline uint32_t millisDeadlineAfter(uint32_t now, uint32_t delay) {
  return static_cast<uint32_t>(now + delay);
}

#endif
