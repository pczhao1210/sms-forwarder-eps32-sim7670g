#include "sms_filter.h"
#include "config_manager.h"
#include "log_manager.h"

SMSFilter smsFilter;
std::vector<String> SMSFilter::whitelist;
std::vector<String> SMSFilter::blockedKeywords;

bool SMSFilter::shouldForwardSMS(const String& sender, const String& content) {
  // 白名单检查
  if (config.smsFilter.whitelistEnabled) {
    if (!isNumberInWhitelist(sender)) {
      logManager.addLog(LOG_INFO, "FILTER", "号码不在白名单: " + sender);
      return false;
    }
  }
  
  // 关键词过滤
  if (config.smsFilter.keywordFilterEnabled) {
    if (containsBlockedKeyword(content)) {
      logManager.addLog(LOG_INFO, "FILTER", "包含屏蔽关键词");
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