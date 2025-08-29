#include "sms_storage.h"

SMSStorage smsStorage;
std::vector<SMSRecord> SMSStorage::smsRecords;
int SMSStorage::nextId = 1;

void SMSStorage::init() {
  loadFromFile();
  
  // 如果没有短信记录，添加一条示例短信
  if (smsRecords.empty()) {
    SMSRecord sample;
    sample.id = nextId++;
    sample.sender = "10086";
    sample.content = "【中国移动】您的话费余额为100.00元，流量剩余2GB，感谢您的使用！";
    sample.timestamp = String(millis());
    sample.forwarded = true;
    smsRecords.push_back(sample);
    saveToFile();
  }
}

void SMSStorage::saveSMS(const String& sender, const String& content, const String& timestamp, bool forwarded) {
  if (smsRecords.size() >= MAX_SMS_COUNT) {
    smsRecords.erase(smsRecords.begin());
  }
  
  SMSRecord record;
  record.id = nextId++;
  record.sender = cleanJsonString(sender);
  record.content = cleanJsonString(content);
  record.timestamp = timestamp;
  record.forwarded = forwarded;
  
  smsRecords.push_back(record);
  saveToFile();
}

std::vector<SMSRecord> SMSStorage::getAllSMS() {
  return smsRecords;
}

void SMSStorage::clearAllSMS() {
  smsRecords.clear();
  nextId = 1;
  saveToFile();
}

int SMSStorage::getSMSCount() {
  return smsRecords.size();
}

SMSRecord SMSStorage::getSMSById(int id) {
  for (const auto& record : smsRecords) {
    if (record.id == id) {
      return record;
    }
  }
  // 返回空记录
  SMSRecord empty;
  empty.id = 0;
  return empty;
}

void SMSStorage::updateSMSForwardStatus(int id, bool forwarded) {
  for (auto& record : smsRecords) {
    if (record.id == id) {
      record.forwarded = forwarded;
      saveToFile();
      break;
    }
  }
}

void SMSStorage::loadFromFile() {
  File file = SPIFFS.open("/sms.json", "r");
  if (!file) {
    return;
  }
  
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    // JSON解析失败，删除损坏的文件
    SPIFFS.remove("/sms.json");
    smsRecords.clear();
    nextId = 1;
    return;
  }
  
  nextId = doc["nextId"] | 1;
  JsonArray smsArray = doc["sms"];
  
  smsRecords.clear();
  for (JsonObject sms : smsArray) {
    SMSRecord record;
    record.id = sms["id"];
    record.sender = sms["sender"].as<String>();
    record.content = sms["content"].as<String>();
    record.timestamp = sms["timestamp"].as<String>();
    record.forwarded = sms["forwarded"];
    smsRecords.push_back(record);
  }
}

String SMSStorage::cleanJsonString(const String& str) {
  String result = "";
  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    // 只保留可打印字符和常见空白字符
    if (c >= 32 && c <= 126) {
      // 转义JSON特殊字符
      if (c == '"') {
        result += "\\\"";
      } else if (c == '\\') {
        result += "\\\\";
      } else {
        result += c;
      }
    } else if (c == '\n') {
      result += "\\n";
    } else if (c == '\r') {
      result += "\\r";
    } else if (c == '\t') {
      result += "\\t";
    }
    // 其他控制字符直接忽略
  }
  return result;
}

void SMSStorage::saveToFile() {
  DynamicJsonDocument doc(8192);
  doc["nextId"] = nextId;
  
  JsonArray smsArray = doc.createNestedArray("sms");
  for (const auto& record : smsRecords) {
    JsonObject sms = smsArray.createNestedObject();
    sms["id"] = record.id;
    sms["sender"] = record.sender;
    sms["content"] = record.content;
    sms["timestamp"] = record.timestamp;
    sms["forwarded"] = record.forwarded;
  }
  
  File file = SPIFFS.open("/sms.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
}