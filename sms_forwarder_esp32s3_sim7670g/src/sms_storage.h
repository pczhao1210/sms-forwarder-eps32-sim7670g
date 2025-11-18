#ifndef SMS_STORAGE_H
#define SMS_STORAGE_H

#include <SPIFFS.h>
#include <ArduinoJson.h>

#define MAX_SMS_COUNT 50

struct SMSRecord {
  int id;
  String sender;
  String content;
  String timestamp;
  bool forwarded;
};

class SMSStorage {
public:
  static void init();
  static void saveSMS(const String& sender, const String& content, const String& timestamp, bool forwarded = false);
  static std::vector<SMSRecord> getAllSMS();
  static SMSRecord getSMSById(int id);
  static void updateSMSForwardStatus(int id, bool forwarded);
  static void clearAllSMS();
  static bool deleteSMS(int id);
  static int getSMSCount();
  
private:
  static int nextId;
  static void loadFromFile();
  static void saveToFile();
  static String cleanJsonString(const String& str);
  static std::vector<SMSRecord> smsRecords;
  static bool deleteByIdInternal(int id);
};

extern SMSStorage smsStorage;

#endif
