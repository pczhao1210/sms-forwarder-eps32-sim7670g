#ifndef VERIFIED_FILE_H
#define VERIFIED_FILE_H

#include <SPIFFS.h>

class VerifiedFileWriter : public Print {
public:
  explicit VerifiedFileWriter(File& file) : file_(file) {}

  size_t write(uint8_t value) override { return write(&value, 1); }

  size_t write(const uint8_t* buffer, size_t size) override {
    for (size_t index = 0; index < size; index++) {
      checksum_ = (checksum_ ^ buffer[index]) * 16777619U;
    }
    expected_ += size;
    size_t written = file_.write(buffer, size);
    complete_ = complete_ && written == size;
    return written;
  }

  bool finish(const char* path) {
    file_.flush();
    file_.close();
    if (!complete_ || expected_ == 0) return false;
    File check = SPIFFS.open(path, "r");
    if (!check) return false;
    bool valid = check.size() == expected_;
    uint32_t checksum = 2166136261U;
    size_t received = 0;
    uint8_t buffer[256];
    while (valid && check.available()) {
      size_t count = check.read(buffer, sizeof(buffer));
      if (count == 0) {
        valid = false;
        break;
      }
      received += count;
      for (size_t index = 0; index < count; index++) {
        checksum = (checksum ^ buffer[index]) * 16777619U;
      }
    }
    check.close();
    return valid && received == expected_ && checksum == checksum_;
  }

private:
  File& file_;
  size_t expected_ = 0;
  uint32_t checksum_ = 2166136261U;
  bool complete_ = true;
};

#endif