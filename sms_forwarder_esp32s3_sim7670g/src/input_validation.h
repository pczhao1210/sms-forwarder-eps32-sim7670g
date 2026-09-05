#ifndef INPUT_VALIDATION_H
#define INPUT_VALIDATION_H

#include <cstring>
#include <cstdint>
#include <climits>

inline bool parsePositiveIdInput(const char* value, size_t length, int& output) {
  if (length == 0) return false;
  int result = 0;
  for (size_t index = 0; index < length; index++) {
    char digit = value[index];
    if (digit < '0' || digit > '9' || result > (INT_MAX - (digit - '0')) / 10) return false;
    result = result * 10 + (digit - '0');
  }
  if (result == 0) return false;
  output = result;
  return true;
}

inline bool parseBooleanInput(const char* value, bool& output) {
  if (!strcmp(value, "true") || !strcmp(value, "1") || !strcmp(value, "on")) {
    output = true;
    return true;
  }
  if (!strcmp(value, "false") || !strcmp(value, "0") || !strcmp(value, "off")) {
    output = false;
    return true;
  }
  return false;
}

inline bool nextUtf8CodePoint(const char* text, size_t length, size_t& offset, uint32_t& codePoint) {
  if (offset >= length) return false;
  uint8_t first = static_cast<uint8_t>(text[offset]);
  size_t continuation = 0;
  uint32_t minimum = 0;
  if (first < 0x80) {
    codePoint = first;
  } else if (first >= 0xC2 && first <= 0xDF) {
    codePoint = first & 0x1F;
    continuation = 1;
    minimum = 0x80;
  } else if (first >= 0xE0 && first <= 0xEF) {
    codePoint = first & 0x0F;
    continuation = 2;
    minimum = 0x800;
  } else if (first >= 0xF0 && first <= 0xF4) {
    codePoint = first & 0x07;
    continuation = 3;
    minimum = 0x10000;
  } else {
    return false;
  }
  if (continuation >= length - offset) return false;
  for (size_t index = 1; index <= continuation; index++) {
    uint8_t value = static_cast<uint8_t>(text[offset + index]);
    if ((value & 0xC0) != 0x80) return false;
    codePoint = (codePoint << 6) | (value & 0x3F);
  }
  if (codePoint < minimum || codePoint > 0x10FFFF ||
      (codePoint >= 0xD800 && codePoint <= 0xDFFF)) return false;
  offset += continuation + 1;
  return true;
}

inline const char* validateSmsInput(const char* phone, size_t phoneLength, const char* content, size_t contentLength) {
  size_t firstDigit = phoneLength && phone[0] == '+' ? 1 : 0;
  if (phoneLength <= firstDigit || phoneLength - firstDigit > 20) return "invalid_phone_number";
  for (size_t index = firstDigit; index < phoneLength; index++) {
    if (phone[index] < '0' || phone[index] > '9') return "invalid_phone_number";
  }
  if (contentLength == 0) return "sms_requires_content";
  size_t offset = 0;
  size_t units = 0;
  while (offset < contentLength) {
    uint32_t codePoint = 0;
    if (!nextUtf8CodePoint(content, contentLength, offset, codePoint)) return "invalid_utf8";
    units += codePoint > 0xFFFF ? 2 : 1;
    if (units > 70) return "sms_too_long";
  }
  return nullptr;
}

inline bool isSafeAtField(const char* value, size_t length) {
  if (length > 100) return false;
  for (size_t index = 0; index < length; index++) {
    unsigned char byte = static_cast<unsigned char>(value[index]);
    if (byte < 32 || byte > 126 || byte == '"' || byte == '\\') return false;
  }
  return true;
}

#endif