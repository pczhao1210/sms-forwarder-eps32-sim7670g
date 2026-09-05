#ifndef AT_RESPONSE_H
#define AT_RESPONSE_H

#include <cstring>

enum class AtResult { Pending, Ok, Error };

inline AtResult classifyAtResult(const char* line) {
  if (strcmp(line, "OK") == 0) return AtResult::Ok;
  if (strcmp(line, "ERROR") == 0 || strncmp(line, "+CME ERROR", 10) == 0 ||
      strncmp(line, "+CMS ERROR", 10) == 0) return AtResult::Error;
  return AtResult::Pending;
}

#endif