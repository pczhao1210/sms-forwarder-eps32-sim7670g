#ifndef HTTP_LIMITS_H
#define HTTP_LIMITS_H

#include <Arduino.h>
#include <Stream.h>
#include <algorithm>
#include <string>
#include "millis_utils.h"

class BoundedHttpResponse : public Stream {
public:
  explicit BoundedHttpResponse(size_t limit = 4096) : limit_(limit) { body_.reserve(limit); }
  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t* data, size_t size) override {
    if (size > limit_ - body_.size()) {
      complete_ = false;
      return 0;
    }
    body_.append(reinterpret_cast<const char*>(data), size);
    return size;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  bool complete() const { return complete_; }
  const std::string& body() const { return body_; }

private:
  size_t limit_;
  bool complete_ = true;
  std::string body_;
};

template <typename BaseClient>
class BoundedHttpClient : public BaseClient {
public:
  explicit BoundedHttpClient(void (*service)() = nullptr) : started_(millis()), service_(service) {}
  using BaseClient::read;
  bool limitExceeded() const { return limitExceeded_; }
  int available() override { return withinBudget() ? BaseClient::available() : 0; }
  uint8_t connected() override { return withinBudget() ? BaseClient::connected() : 0; }
  int read() override {
    uint8_t value;
    return read(&value, 1) == 1 ? value : -1;
  }
  int read(uint8_t* buffer, size_t size) override {
    if (!withinBudget()) return -1;
    size = std::min(size, kMaxReceived - received_);
    int count = BaseClient::read(buffer, size);
    if (count > 0) received_ += count;
    return count;
  }
  size_t readBytes(char* buffer, size_t length) override {
    size_t received = 0;
    uint32_t lastData = millis();
    while (received < length && withinBudget()) {
      int count = read(reinterpret_cast<uint8_t*>(buffer + received), length - received);
      if (count > 0) {
        received += count;
        lastData = millis();
      } else if (!connected() || millisElapsed(millis(), lastData, 2000UL)) {
        break;
      } else {
        delay(1);
      }
    }
    return received;
  }
  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t* data, size_t size) override {
    return withinBudget() ? BaseClient::write(data, size) : 0;
  }

private:
  bool withinBudget() {
    if (service_) service_();
    if (received_ >= kMaxReceived || millisElapsed(millis(), started_, 10000UL)) {
      limitExceeded_ = true;
      BaseClient::stop();
      return false;
    }
    return true;
  }
  static constexpr size_t kMaxReceived = 12288;
  uint32_t started_;
  size_t received_ = 0;
  bool limitExceeded_ = false;
  void (*service_)();
};

#endif