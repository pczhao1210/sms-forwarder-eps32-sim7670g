#include <cassert>
#include <iostream>
#include <string>
#include "../sms_forwarder_esp32s3_sim7670g/src/input_validation.h"

static const char* validate(const std::string& phone, const std::string& message) {
  return validateSmsInput(phone.data(), phone.size(), message.data(), message.size());
}

int main() {
  int id = 0;
  assert(parsePositiveIdInput("2147483647", 10, id) && id == INT_MAX);
  for (const char* invalid : {"", "0", "-1", "+1", "12x", " 1", "2147483648", "9999999999999999999999999999"}) {
    assert(!parsePositiveIdInput(invalid, strlen(invalid), id));
  }
  assert(!parsePositiveIdInput("12\0x", 4, id));
  assert(parsePositiveIdInput("42", 2, id) && id == 42);
  bool value = true;
  assert(parseBooleanInput("false", value) && !value);
  assert(parseBooleanInput("on", value) && value);
  assert(!parseBooleanInput("yes please", value));
  assert(!validate("+44123456789", std::string(70, 'a')));
  assert(validate("+44123456789", std::string(71, 'a')));
  assert(!validate("43430", "balance"));
  assert(validate("12x34", "message"));
  assert(validate("++123", "message"));
  assert(validate("+", "message"));
  assert(validate(std::string("123\0xxx", 7), "message"));
  assert(validate("123", ""));
  assert(validate("123", "\xC0\xAF"));
  assert(validate("123", "\xED\xA0\x80"));
  assert(validate("123", "\xF4\x90\x80\x80"));
  assert(validate("123", "\xE4\xB8"));
  std::string emoji;
  for (int index = 0; index < 35; index++) emoji += "\xF0\x9F\x98\x80";
  assert(!validate("123", emoji));
  emoji += "\xF0\x9F\x98\x80";
  assert(validate("123", emoji));
  assert(isSafeAtField("giffgaff.com", 12));
  assert(!isSafeAtField("bad\r\n", 5));
  std::cout << "Input validation tests passed.\n";
}