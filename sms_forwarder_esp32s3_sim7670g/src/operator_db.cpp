#include "operator_db.h"
#include <string.h>

struct OperatorEntry {
  const char* code;
  const char* nameZh;
  const char* nameEn;
};

static const OperatorEntry OPERATOR_TABLE[] = {
  {"23410", "giffgaff", "giffgaff"},
  {"46000", "\u4e2d\u56fd\u79fb\u52a8", "China Mobile"},
  {"46001", "\u4e2d\u56fd\u8054\u901a", "China Unicom"},
  {"46002", "\u4e2d\u56fd\u79fb\u52a8", "China Mobile"},
  {"46003", "\u4e2d\u56fd\u7535\u4fe1", "China Telecom"},
  {"46004", "\u4e2d\u56fd\u79fb\u52a8", "China Mobile"},
  {"46005", "\u4e2d\u56fd\u7535\u4fe1", "China Telecom"},
  {"46006", "\u4e2d\u56fd\u8054\u901a", "China Unicom"},
  {"46007", "\u4e2d\u56fd\u79fb\u52a8", "China Mobile"},
  {"46009", "\u4e2d\u56fd\u8054\u901a", "China Unicom"},
  {"46011", "\u4e2d\u56fd\u7535\u4fe1", "China Telecom"}
};

static const size_t OPERATOR_COUNT = sizeof(OPERATOR_TABLE) / sizeof(OPERATOR_TABLE[0]);

static bool isDigitsOnly(const String& value) {
  if (value.isEmpty()) return false;
  for (int i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (c < '0' || c > '9') return false;
  }
  return true;
}

String getOperatorNameByCode(const String& code, const char* lang) {
  if (code.isEmpty()) return "";
  String trimmed = code;
  trimmed.trim();
  if (!isDigitsOnly(trimmed)) {
    return code;
  }
  for (size_t i = 0; i < OPERATOR_COUNT; i++) {
    if (trimmed == OPERATOR_TABLE[i].code) {
      if (lang && strcmp(lang, "en") == 0) {
        return String(OPERATOR_TABLE[i].nameEn);
      }
      return String(OPERATOR_TABLE[i].nameZh);
    }
  }
  return code;
}

bool isKnownOperatorCode(const String& code) {
  if (code.isEmpty()) return false;
  String trimmed = code;
  trimmed.trim();
  if (!isDigitsOnly(trimmed)) return false;
  for (size_t i = 0; i < OPERATOR_COUNT; i++) {
    if (trimmed == OPERATOR_TABLE[i].code) {
      return true;
    }
  }
  return false;
}
