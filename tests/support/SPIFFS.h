#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <type_traits>

struct String : std::string {
  using std::string::string;
  String() = default;
  String(const std::string& value) : std::string(value) {}
  template <typename Value, std::enable_if_t<std::is_integral_v<Value>, int> = 0>
  String(Value value) : std::string(std::to_string(value)) {}
  bool isEmpty() const { return empty(); }
  char charAt(size_t index) const { return at(index); }
  bool startsWith(const char* prefix) const { return compare(0, strlen(prefix), prefix) == 0; }
  bool equals(const char* other) const { return *this == other; }
  int indexOf(const char* text, size_t offset = 0) const { auto found = find(text, offset); return found == npos ? -1 : static_cast<int>(found); }
  int indexOf(const String& text, size_t offset = 0) const { return indexOf(text.c_str(), offset); }
  int indexOf(char value, size_t offset = 0) const { auto found = find(value, offset); return found == npos ? -1 : static_cast<int>(found); }
  int lastIndexOf(char value) const { auto found = rfind(value); return found == npos ? -1 : static_cast<int>(found); }
  String substring(size_t start, size_t end = npos) const { return substr(start, end == npos ? npos : end - start); }
  long toInt() const { return strtol(c_str(), nullptr, 10); }
  void trim() {
    size_t first = find_first_not_of(" \r\n\t");
    if (first == npos) { clear(); return; }
    size_t last = find_last_not_of(" \r\n\t");
    *this = substr(first, last - first + 1);
  }
};

class Print {
public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t value) = 0;
  virtual size_t write(const uint8_t* data, size_t length) {
    size_t written = 0;
    while (written < length && write(data[written])) written++;
    return written;
  }
  size_t print(const char* value) { return write(reinterpret_cast<const uint8_t*>(value), strlen(value)); }
  size_t print(char value) { return write(static_cast<uint8_t>(value)); }
  size_t print(int value) { return print(std::to_string(value).c_str()); }
};

namespace FakeFS {
inline size_t writeRemaining = std::numeric_limits<size_t>::max();
inline bool failOpen = false;
inline bool corruptOnFlush = false;
inline std::string failRenameTo;
}

class File : public Print {
public:
  File() = default;
  explicit File(std::shared_ptr<std::string> contents) : contents_(contents) {}
  explicit operator bool() const { return static_cast<bool>(contents_); }
  size_t size() const { return contents_ ? contents_->size() : 0; }
  int available() const { return contents_ && offset_ < size(); }
  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t* data, size_t length) override {
    if (!contents_) return 0;
    size_t count = std::min(length, FakeFS::writeRemaining);
    contents_->append(reinterpret_cast<const char*>(data), count);
    FakeFS::writeRemaining -= count;
    return count;
  }
  int read() { return available() ? static_cast<uint8_t>((*contents_)[offset_++]) : -1; }
  size_t read(uint8_t* buffer, size_t length) {
    size_t count = 0;
    while (count < length && available()) buffer[count++] = static_cast<uint8_t>(read());
    return count;
  }
  size_t readBytes(char* buffer, size_t length) { return read(reinterpret_cast<uint8_t*>(buffer), length); }
  void flush() {
    if (FakeFS::corruptOnFlush && contents_ && !contents_->empty()) (*contents_)[0] = '!';
  }
  void close() { contents_.reset(); }
private:
  std::shared_ptr<std::string> contents_;
  size_t offset_ = 0;
};

struct FakeSPIFFS {
  std::map<std::string, std::shared_ptr<std::string>> files;
  bool exists(const char* name) const { return files.count(name) != 0; }
  bool remove(const char* name) { return files.erase(name) != 0; }
  File open(const char* name, const char* mode) {
    if (FakeFS::failOpen) return {};
    if (mode[0] == 'w') files[name] = std::make_shared<std::string>();
    auto entry = files.find(name);
    return entry == files.end() ? File{} : File(entry->second);
  }
  bool rename(const char* from, const char* to) {
    if (!exists(from) || exists(to) || FakeFS::failRenameTo == to) return false;
    files[to] = files.at(from);
    files.erase(from);
    return true;
  }
};

inline FakeSPIFFS SPIFFS;