#ifndef SMS_STORAGE_H
#define SMS_STORAGE_H

#include <SPIFFS.h>
#include <ArduinoJson.h>

#define MAX_SMS_COUNT 50

namespace SMSStatus {
  static const char* const RECEIVED = "received";
  static const char* const INVALID = "invalid";
  static const char* const FILTERED = "filtered";
  static const char* const PENDING_FORWARD = "pending_forward";
  static const char* const FORWARD_SUCCESS = "forward_success";
  static const char* const FORWARD_FAILED = "forward_failed";
  static const char* const RETRY_SCHEDULED = "retry_scheduled";
  static const char* const RETRYING = "retrying";
  static const char* const RETRY_EXHAUSTED = "retry_exhausted";
  static const char* const MANUAL_FORWARD_SUCCESS = "manual_forward_success";
}

struct SMSRecord {
  int id;
  String sender;
  String content;
  String timestamp;
  String status;
  int retryCount;
  String lastAttemptAt;
  String lastError;
};

class SMSStorage {
public:
  static void init();
  static int saveSMS(const String& sender, const String& content, const String& timestamp, const String& status = SMSStatus::RECEIVED);
  static std::vector<SMSRecord> getAllSMS();
  static SMSRecord getSMSById(int id);
  static bool updateSMSStatus(int id, const String& status, const String& lastAttemptAt = "", const String& lastError = "", int retryCount = -1);
  static void clearAllSMS();
  static bool deleteSMS(int id);
  static int getSMSCount();
  static bool isSuccessStatus(const String& status);
  static bool canManualForward(const String& status);
  
private:
  static int nextId;
  static void loadFromFile();
  static bool saveToFile();
  static std::vector<SMSRecord> smsRecords;
  static bool deleteByIdInternal(int id);
};

extern SMSStorage smsStorage;

#endif
