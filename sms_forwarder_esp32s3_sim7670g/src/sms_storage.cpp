#include "sms_storage.h"

SMSStorage smsStorage;
std::vector<SMSRecord> SMSStorage::smsRecords;
int SMSStorage::nextId = 1;

static const char* SMS_FILE_PATH = "/sms.json";

void SMSStorage::init() {
  loadFromFile();

  if (smsRecords.empty()) {
    SMSRecord sample;
    sample.id = nextId++;
    sample.sender = "10086";
    sample.content = "【中国移动】您的话费余额为100.00元，流量剩余2GB，感谢您的使用！";
    sample.timestamp = String(millis());
    sample.status = SMSStatus::FORWARD_SUCCESS;
    sample.retryCount = 0;
    sample.lastAttemptAt = sample.timestamp;
    sample.lastError = "";
    smsRecords.push_back(sample);
    saveToFile();
  }
}

int SMSStorage::saveSMS(const String& sender, const String& content, const String& timestamp, const String& status) {
  if (smsRecords.size() >= MAX_SMS_COUNT) {
    smsRecords.erase(smsRecords.begin());
  }

  SMSRecord record;
  record.id = nextId++;
  record.sender = sender;
  record.content = content;
  record.timestamp = timestamp;
  record.status = status;
  record.retryCount = 0;
  record.lastAttemptAt = "";
  record.lastError = "";

  smsRecords.push_back(record);
  saveToFile();
  return record.id;
}

std::vector<SMSRecord> SMSStorage::getAllSMS() {
  return smsRecords;
}

void SMSStorage::clearAllSMS() {
  smsRecords.clear();
  nextId = 1;
  saveToFile();
}

bool SMSStorage::deleteSMS(int id) {
  bool removed = deleteByIdInternal(id);
  if (removed) {
    saveToFile();
  }
  return removed;
}

int SMSStorage::getSMSCount() {
  return static_cast<int>(smsRecords.size());
}

SMSRecord SMSStorage::getSMSById(int id) {
  for (const auto& record : smsRecords) {
    if (record.id == id) {
      return record;
    }
  }
  SMSRecord empty;
  empty.id = 0;
  empty.retryCount = 0;
  return empty;
}

bool SMSStorage::updateSMSStatus(int id, const String& status, const String& lastAttemptAt, const String& lastError, int retryCount) {
  for (auto& record : smsRecords) {
    if (record.id != id) continue;
    record.status = status;
    if (!lastAttemptAt.isEmpty()) {
      record.lastAttemptAt = lastAttemptAt;
    }
    record.lastError = lastError;
    if (retryCount >= 0) {
      record.retryCount = retryCount;
    }
    saveToFile();
    return true;
  }
  return false;
}

bool SMSStorage::isSuccessStatus(const String& status) {
  return status == SMSStatus::FORWARD_SUCCESS || status == SMSStatus::MANUAL_FORWARD_SUCCESS;
}

bool SMSStorage::canManualForward(const String& status) {
  return !isSuccessStatus(status) &&
         status != SMSStatus::INVALID &&
         status != SMSStatus::PENDING_FORWARD &&
         status != SMSStatus::RETRYING;
}

void SMSStorage::loadFromFile() {
  File file = SPIFFS.open(SMS_FILE_PATH, "r");
  if (!file) {
    return;
  }

  DynamicJsonDocument doc(32768);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    SPIFFS.remove(SMS_FILE_PATH);
    smsRecords.clear();
    nextId = 1;
    return;
  }

  nextId = doc["nextId"] | 1;
  JsonArray smsArray = doc["sms"].as<JsonArray>();

  smsRecords.clear();
  for (JsonObject sms : smsArray) {
    SMSRecord record;
    record.id = sms["id"] | 0;
    record.sender = sms["sender"].as<String>();
    record.content = sms["content"].as<String>();
    record.timestamp = sms["timestamp"].as<String>();
    if (!sms["status"].isNull()) {
      record.status = sms["status"].as<String>();
    } else {
      bool forwarded = sms["forwarded"] | false;
      record.status = forwarded ? String(SMSStatus::FORWARD_SUCCESS) : String(SMSStatus::RECEIVED);
    }
    record.retryCount = sms["retryCount"] | 0;
    record.lastAttemptAt = sms["lastAttemptAt"].as<String>();
    record.lastError = sms["lastError"].as<String>();
    smsRecords.push_back(record);
  }
}

void SMSStorage::saveToFile() {
  DynamicJsonDocument doc(32768);
  doc["nextId"] = nextId;

  JsonArray smsArray = doc.createNestedArray("sms");
  for (const auto& record : smsRecords) {
    JsonObject sms = smsArray.createNestedObject();
    sms["id"] = record.id;
    sms["sender"] = record.sender;
    sms["content"] = record.content;
    sms["timestamp"] = record.timestamp;
    sms["status"] = record.status;
    sms["retryCount"] = record.retryCount;
    sms["lastAttemptAt"] = record.lastAttemptAt;
    sms["lastError"] = record.lastError;
    sms["forwarded"] = isSuccessStatus(record.status);
  }

  File file = SPIFFS.open(SMS_FILE_PATH, "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
}

bool SMSStorage::deleteByIdInternal(int id) {
  for (auto it = smsRecords.begin(); it != smsRecords.end(); ++it) {
    if (it->id == id) {
      smsRecords.erase(it);
      return true;
    }
  }
  return false;
}
