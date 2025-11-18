#ifndef SMS_FILTER_H
#define SMS_FILTER_H

#include <vector>
#include <Arduino.h>

class SMSFilter {
public:
  static bool shouldForwardSMS(const String& sender, const String& content);
  static void addWhitelistNumber(const String& number);
  static void removeWhitelistNumber(const String& number);
  static void addBlockedKeyword(const String& keyword);
  static void removeBlockedKeyword(const String& keyword);
  static void clearWhitelist();
  static void clearBlockedKeywords();
  static std::vector<String> getWhitelist();
  static std::vector<String> getBlockedKeywords();
  static void loadFromConfigStrings(const String& whitelistStr, const String& blockedStr);
  
private:
  static void parseListString(const String& source, std::vector<String>& target);
  static bool isNumberInWhitelist(const String& number);
  static bool containsBlockedKeyword(const String& content);
  static std::vector<String> whitelist;
  static std::vector<String> blockedKeywords;
};

extern SMSFilter smsFilter;

#endif
