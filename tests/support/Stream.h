#ifndef TEST_STREAM_H
#define TEST_STREAM_H

#include "SPIFFS.h"

class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual void flush() = 0;
  virtual size_t readBytes(char*, size_t) { return 0; }
};

#endif