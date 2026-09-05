#include "sms_storage.h"
#include "verified_file.h"
#include <algorithm>
#include <limits.h>
#include <string.h>

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

static size_t getSmsJsonFileSize(const char* path) {
  File file = SPIFFS.open(path, "r");
  if (!file) return 0;
  size_t fileSize = file.size();
  file.close();
  return fileSize;
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

static void writeJsonString(Print& file, const String& value) {
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
  if (nextId <= 0 || nextId == INT_MAX) return 0;
  std::vector<std::pair<size_t, SMSRecord>> evicted;

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
  while (smsRecords.size() > MAX_SMS_COUNT ||
         estimateSmsJsonSize(smsRecords) + 1024 > SMS_JSON_MAX_FILE_SIZE) {
    auto candidate = std::find_if(smsRecords.begin(), smsRecords.end(), [&](const SMSRecord& entry) {
      return entry.id != recordId && isTerminalStatus(entry.status);
    });
    if (candidate == smsRecords.end()) break;
    evicted.emplace_back(static_cast<size_t>(candidate - smsRecords.begin()), *candidate);
    smsRecords.erase(candidate);
  }
  bool fits = smsRecords.size() <= MAX_SMS_COUNT &&
              estimateSmsJsonSize(smsRecords) + 1024 <= SMS_JSON_MAX_FILE_SIZE;
  if (!fits || !saveToFile()) {
    deleteByIdInternal(recordId);
    for (auto entry = evicted.rbegin(); entry != evicted.rend(); ++entry) {
      smsRecords.insert(smsRecords.begin() + entry->first, entry->second);
    }
    nextId = recordId;
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

bool SMSStorage::clearAllSMS() {
  std::vector<SMSRecord> previousRecords = smsRecords;
  int previousNextId = nextId;
  smsRecords.clear();
  nextId = 1;
  if (saveToFile()) return true;
  smsRecords = previousRecords;
  nextId = previousNextId;
  return false;
}

bool SMSStorage::deleteSMS(int id) {
  for (auto it = smsRecords.begin(); it != smsRecords.end(); ++it) {
    if (it->id != id) continue;
    size_t index = static_cast<size_t>(it - smsRecords.begin());
    SMSRecord removedRecord = *it;
    smsRecords.erase(it);
    if (saveToFile()) return true;
    smsRecords.insert(smsRecords.begin() + index, removedRecord);
    return false;
  }
  return false;
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

bool SMSStorage::updateSMSStatus(int id, const String& status, const String& lastAttemptAt, const String& lastError, int retryCount, bool persist) {
  for (auto& record : smsRecords) {
    if (record.id != id) continue;
    SMSRecord updatedRecord = record;
    updatedRecord.status = status;
    if (!lastAttemptAt.isEmpty()) {
      updatedRecord.lastAttemptAt = lastAttemptAt;
    }
    updatedRecord.lastError = lastError;
    if (retryCount >= 0) {
      updatedRecord.retryCount = retryCount;
    }

    bool changed = updatedRecord.status != record.status ||
                   updatedRecord.lastAttemptAt != record.lastAttemptAt ||
                   updatedRecord.lastError != record.lastError ||
                   updatedRecord.retryCount != record.retryCount;
    if (!changed) return true;

    SMSRecord previousRecord = record;
    record = updatedRecord;
    if (!persist) return true;
    if (!saveToFile()) {
      record = previousRecord;
      return false;
    }
    return true;
  }
  return false;
}

bool SMSStorage::flush() {
  return saveToFile();
}

bool SMSStorage::isSuccessStatus(const String& status) {
  return status == SMSStatus::FORWARD_SUCCESS || status == SMSStatus::MANUAL_FORWARD_SUCCESS;
}

bool SMSStorage::isTerminalStatus(const String& status) {
  return isSuccessStatus(status) || status == SMSStatus::INVALID ||
         status == SMSStatus::FILTERED || status == SMSStatus::RETRY_EXHAUSTED;
}

bool SMSStorage::canManualForward(const String& status) {
  return !isSuccessStatus(status) &&
         status != SMSStatus::INVALID &&
         status != SMSStatus::PENDING_FORWARD &&
         status != SMSStatus::RETRYING;
}

void SMSStorage::loadFromFile() {
  size_t maxFileSize = getSmsJsonFileSize(SMS_FILE_PATH);
  size_t backupSize = getSmsJsonFileSize(SMS_BACKUP_FILE_PATH);
  size_t temporarySize = getSmsJsonFileSize(SMS_TMP_FILE_PATH);
  if (backupSize > maxFileSize) maxFileSize = backupSize;
  if (temporarySize > maxFileSize) maxFileSize = temporarySize;

  DynamicJsonDocument doc(boundedSmsJsonCapacity(maxFileSize));
  auto loadCandidate = [&](const char* path) {
    File file = SPIFFS.open(path, "r");
    if (!file) return false;
    doc.clear();
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    return !error && !doc.overflowed() && doc["sms"].is<JsonArray>();
  };

  const char* loadedPath = nullptr;
  if (loadCandidate(SMS_FILE_PATH)) {
    loadedPath = SMS_FILE_PATH;
  } else if (loadCandidate(SMS_BACKUP_FILE_PATH)) {
    loadedPath = SMS_BACKUP_FILE_PATH;
  } else if (loadCandidate(SMS_TMP_FILE_PATH)) {
    loadedPath = SMS_TMP_FILE_PATH;
  }

  if (!loadedPath) {
    SPIFFS.remove(SMS_CORRUPT_FILE_PATH);
    if (SPIFFS.exists(SMS_FILE_PATH)) {
      SPIFFS.rename(SMS_FILE_PATH, SMS_CORRUPT_FILE_PATH);
    }
    SPIFFS.remove(SMS_BACKUP_FILE_PATH);
    SPIFFS.remove(SMS_TMP_FILE_PATH);
    smsRecords.clear();
    nextId = 1;
    return;
  }

  if (strcmp(loadedPath, SMS_FILE_PATH) != 0) {
    if (SPIFFS.exists(SMS_FILE_PATH)) {
      SPIFFS.remove(SMS_CORRUPT_FILE_PATH);
      SPIFFS.rename(SMS_FILE_PATH, SMS_CORRUPT_FILE_PATH);
    }
    bool promoted = !SPIFFS.exists(SMS_FILE_PATH) && SPIFFS.rename(loadedPath, SMS_FILE_PATH);
    if (promoted) {
      SPIFFS.remove(SMS_BACKUP_FILE_PATH);
      SPIFFS.remove(SMS_TMP_FILE_PATH);
    }
  } else {
    SPIFFS.remove(SMS_BACKUP_FILE_PATH);
    SPIFFS.remove(SMS_TMP_FILE_PATH);
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
    nextId = maxId < INT_MAX ? maxId + 1 : INT_MAX;
  }
}

bool SMSStorage::saveToFile() {
  if (estimateSmsJsonSize(smsRecords) > SMS_JSON_MAX_FILE_SIZE) return false;
  SPIFFS.remove(SMS_TMP_FILE_PATH);
  File file = SPIFFS.open(SMS_TMP_FILE_PATH, "w");
  if (!file) return false;
  VerifiedFileWriter writer(file);
  writer.print("{\"nextId\":");
  writer.print(nextId);
  writer.print(",\"sms\":[");
  bool first = true;
  for (const auto& record : smsRecords) {
    if (!first) writer.print(',');
    writer.print("{\"id\":");
    writer.print(record.id);
    writer.print(",\"sender\":");
    writeJsonString(writer, record.sender);
    writer.print(",\"content\":");
    writeJsonString(writer, record.content);
    writer.print(",\"timestamp\":");
    writeJsonString(writer, record.timestamp);
    writer.print(",\"status\":");
    writeJsonString(writer, record.status);
    writer.print(",\"retryCount\":");
    writer.print(record.retryCount);
    writer.print(",\"lastAttemptAt\":");
    writeJsonString(writer, record.lastAttemptAt);
    writer.print(",\"lastError\":");
    writeJsonString(writer, record.lastError);
    writer.print(",\"forwarded\":");
    writer.print(isSuccessStatus(record.status) ? "true" : "false");
    writer.print('}');
    first = false;
  }
  writer.print("]}");
  if (!writer.finish(SMS_TMP_FILE_PATH)) {
    SPIFFS.remove(SMS_TMP_FILE_PATH);
    return false;
  }

  bool hadExisting = SPIFFS.exists(SMS_FILE_PATH);
  if (hadExisting) {
    SPIFFS.remove(SMS_BACKUP_FILE_PATH);
    if (!SPIFFS.rename(SMS_FILE_PATH, SMS_BACKUP_FILE_PATH)) {
      SPIFFS.remove(SMS_TMP_FILE_PATH);
      return false;
    }
  }
  if (!SPIFFS.rename(SMS_TMP_FILE_PATH, SMS_FILE_PATH)) {
    if (hadExisting) SPIFFS.rename(SMS_BACKUP_FILE_PATH, SMS_FILE_PATH);
    SPIFFS.remove(SMS_TMP_FILE_PATH);
    return false;
  }

  SPIFFS.remove(SMS_BACKUP_FILE_PATH);
  return true;
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
