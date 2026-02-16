#include "sms_filter.h"
#include "config_manager.h"
#include "log_manager.h"

SMSFilter smsFilter;
std::vector<String> SMSFilter::whitelist;
std::vector<String> SMSFilter::blockedKeywords;

void SMSFilter::parseListString(const String& source, std::vector<String>& target) {
  target.clear();
  String normalized = source;
  normalized.replace(",", "\n");
  normalized.replace("\r", "\n");
  
  int start = 0;
  while (start < normalized.length()) {
    int end = normalized.indexOf('\n', start);
    if (end < 0) end = normalized.length();
    
    String item = normalized.substring(start, end);
    item.trim();
    if (!item.isEmpty()) {
      target.push_back(item);
    }
    
    start = end + 1;
  }
}

void SMSFilter::loadFromConfigStrings(const String& whitelistStr, const String& blockedStr) {
  parseListString(whitelistStr, whitelist);
  parseListString(blockedStr, blockedKeywords);
  
  LOGI("FILTER", "filter_counts", String(whitelist.size()).c_str(), String(blockedKeywords.size()).c_str());
}

bool SMSFilter::shouldForwardSMS(const String& sender, const String& content) {
  // 白名单检查
  if (config.smsFilter.whitelistEnabled) {
    if (!isNumberInWhitelist(sender)) {
      LOGI("FILTER", "filter_not_in_whitelist", sender.c_str());
      return false;
    }
  }
  
  // 关键词过滤
  if (config.smsFilter.keywordFilterEnabled) {
    if (containsBlockedKeyword(content)) {
      LOGI("FILTER", "filter_blocked_keyword");
      return false;
    }
  }
  
  return true;
}

bool SMSFilter::isNumberInWhitelist(const String& number) {
  for (const String& whiteNumber : whitelist) {
    if (number.indexOf(whiteNumber) >= 0) {
      return true;
    }
  }
  return false;
}

bool SMSFilter::containsBlockedKeyword(const String& content) {
  for (const String& keyword : blockedKeywords) {
    if (content.indexOf(keyword) >= 0) {
      return true;
    }
  }
  return false;
}

void SMSFilter::addWhitelistNumber(const String& number) {
  whitelist.push_back(number);
}

void SMSFilter::addBlockedKeyword(const String& keyword) {
  blockedKeywords.push_back(keyword);
}

void SMSFilter::clearWhitelist() {
  whitelist.clear();
}

void SMSFilter::clearBlockedKeywords() {
  blockedKeywords.clear();
}

std::vector<String> SMSFilter::getWhitelist() {
  return whitelist;
}

std::vector<String> SMSFilter::getBlockedKeywords() {
  return blockedKeywords;
}
