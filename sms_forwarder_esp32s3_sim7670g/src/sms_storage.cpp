#include "sms_storage.h"

SMSStorage smsStorage;
std::vector<SMSRecord> SMSStorage::smsRecords;
int SMSStorage::nextId = 1;

static const char* SMS_FILE_PATH = "/sms.json";
static const char* SMS_TMP_FILE_PATH = "/sms.tmp";
static const char* SMS_BACKUP_FILE_PATH = "/sms.bak";
static const char* SMS_CORRUPT_FILE_PATH = "/sms.corrupt";
static const size_t SMS_JSON_CAPACITY = 65536;

void SMSStorage::init() {
  loadFromFile();
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

  int recordId = record.id;
  smsRecords.push_back(record);
  if (!saveToFile()) {
    deleteByIdInternal(recordId);
    return 0;
  }
  return recordId;
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

  DynamicJsonDocument doc(SMS_JSON_CAPACITY);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error || doc.overflowed()) {
    SPIFFS.remove(SMS_CORRUPT_FILE_PATH);
    SPIFFS.rename(SMS_FILE_PATH, SMS_CORRUPT_FILE_PATH);
    smsRecords.clear();
    nextId = 1;
    return;
  }

  nextId = doc["nextId"] | 1;
  JsonArray smsArray = doc["sms"].as<JsonArray>();

  smsRecords.clear();
  int maxId = 0;
  for (JsonObject sms : smsArray) {
    SMSRecord record;
    record.id = sms["id"] | 0;
    if (record.id <= 0) continue;
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
    if (record.id > maxId) maxId = record.id;
  }

  if (nextId <= maxId) {
    nextId = maxId + 1;
  }
}

bool SMSStorage::saveToFile() {
  while (true) {
    DynamicJsonDocument doc(SMS_JSON_CAPACITY);
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

    if (doc.overflowed()) {
      if (smsRecords.empty()) return false;
      smsRecords.erase(smsRecords.begin());
      continue;
    }

    SPIFFS.remove(SMS_TMP_FILE_PATH);
    File file = SPIFFS.open(SMS_TMP_FILE_PATH, "w");
    if (!file) return false;

    size_t bytesWritten = serializeJson(doc, file);
    file.flush();
    file.close();
    if (bytesWritten == 0) {
      SPIFFS.remove(SMS_TMP_FILE_PATH);
      return false;
    }

    SPIFFS.remove(SMS_BACKUP_FILE_PATH);
    bool hadExisting = SPIFFS.exists(SMS_FILE_PATH);
    if (hadExisting && !SPIFFS.rename(SMS_FILE_PATH, SMS_BACKUP_FILE_PATH)) {
      SPIFFS.remove(SMS_TMP_FILE_PATH);
      return false;
    }

    if (!SPIFFS.rename(SMS_TMP_FILE_PATH, SMS_FILE_PATH)) {
      if (hadExisting) {
        SPIFFS.rename(SMS_BACKUP_FILE_PATH, SMS_FILE_PATH);
      }
      SPIFFS.remove(SMS_TMP_FILE_PATH);
      return false;
    }

    SPIFFS.remove(SMS_BACKUP_FILE_PATH);
    return true;
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
