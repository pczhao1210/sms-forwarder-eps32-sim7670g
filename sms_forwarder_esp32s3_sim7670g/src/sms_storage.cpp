#include "sms_storage.h"

SMSStorage smsStorage;
std::vector<SMSRecord> SMSStorage::smsRecords;
int SMSStorage::nextId = 1;

static const char* SMS_FILE_PATH = "/sms.json";
static const char* SMS_TMP_FILE_PATH = "/sms.tmp";
static const char* SMS_BACKUP_FILE_PATH = "/sms.bak";
static const char* SMS_CORRUPT_FILE_PATH = "/sms.corrupt";
static const size_t SMS_JSON_CAPACITY = 65536;
static const size_t SMS_JSON_MIN_CAPACITY = 4096;
static const size_t SMS_JSON_MAX_FILE_SIZE = 49152;

static size_t boundedSmsJsonCapacity(size_t fileSize) {
  size_t capacity = fileSize + (fileSize / 2) + 2048;
  if (capacity < SMS_JSON_MIN_CAPACITY) capacity = SMS_JSON_MIN_CAPACITY;
  if (capacity > SMS_JSON_CAPACITY) capacity = SMS_JSON_CAPACITY;
  return capacity;
}

static size_t escapedJsonLength(const String& value) {
  size_t length = 2;
  for (int i = 0; i < value.length(); i++) {
    unsigned char c = static_cast<unsigned char>(value.charAt(i));
    switch (c) {
      case '\\':
      case '"':
      case '\n':
      case '\r':
      case '\t':
      case '\b':
      case '\f':
        length += 2;
        break;
      default:
        if (c < 32) {
          length += 6;
        } else {
          length++;
        }
        break;
    }
  }
  return length;
}

static void writeJsonString(File& file, const String& value) {
  file.print('"');
  for (int i = 0; i < value.length(); i++) {
    unsigned char c = static_cast<unsigned char>(value.charAt(i));
    switch (c) {
      case '\\': file.print("\\\\"); break;
      case '"': file.print("\\\""); break;
      case '\n': file.print("\\n"); break;
      case '\r': file.print("\\r"); break;
      case '\t': file.print("\\t"); break;
      case '\b': file.print("\\b"); break;
      case '\f': file.print("\\f"); break;
      default:
        if (c < 32) {
          char escaped[7];
          snprintf(escaped, sizeof(escaped), "\\u%04x", c);
          file.print(escaped);
        } else {
          file.print(static_cast<char>(c));
        }
        break;
    }
  }
  file.print('"');
}

static size_t estimateSmsJsonSize(const std::vector<SMSRecord>& records) {
  size_t size = 24;
  for (const auto& record : records) {
    size += 128;
    size += escapedJsonLength(record.sender);
    size += escapedJsonLength(record.content);
    size += escapedJsonLength(record.timestamp);
    size += escapedJsonLength(record.status);
    size += escapedJsonLength(record.lastAttemptAt);
    size += escapedJsonLength(record.lastError);
  }
  return size;
}

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

bool SMSStorage::getSMSAt(size_t index, SMSRecord& recordOut) {
  if (index >= smsRecords.size()) return false;
  recordOut = smsRecords[index];
  return true;
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

  DynamicJsonDocument doc(boundedSmsJsonCapacity(file.size()));
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
    if (estimateSmsJsonSize(smsRecords) > SMS_JSON_MAX_FILE_SIZE) {
      if (smsRecords.empty()) return false;
      smsRecords.erase(smsRecords.begin());
      continue;
    }

    SPIFFS.remove(SMS_TMP_FILE_PATH);
    File file = SPIFFS.open(SMS_TMP_FILE_PATH, "w");
    if (!file) return false;

    file.print("{\"nextId\":");
    file.print(nextId);
    file.print(",\"sms\":[");
    bool first = true;
    for (const auto& record : smsRecords) {
      if (!first) file.print(',');
      file.print("{\"id\":");
      file.print(record.id);
      file.print(",\"sender\":");
      writeJsonString(file, record.sender);
      file.print(",\"content\":");
      writeJsonString(file, record.content);
      file.print(",\"timestamp\":");
      writeJsonString(file, record.timestamp);
      file.print(",\"status\":");
      writeJsonString(file, record.status);
      file.print(",\"retryCount\":");
      file.print(record.retryCount);
      file.print(",\"lastAttemptAt\":");
      writeJsonString(file, record.lastAttemptAt);
      file.print(",\"lastError\":");
      writeJsonString(file, record.lastError);
      file.print(",\"forwarded\":");
      file.print(isSuccessStatus(record.status) ? "true" : "false");
      file.print('}');
      first = false;
    }
    file.print("]}");
    file.flush();
    size_t bytesWritten = file.size();
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
