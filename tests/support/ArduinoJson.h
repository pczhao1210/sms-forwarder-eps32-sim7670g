#pragma once
#include "SPIFFS.h"
#include ARDUINOJSON_HEADER

namespace ArduinoJson {
template <> struct Converter<String> {
  static String fromJson(JsonVariantConst source) {
    const char* value = source.as<const char*>();
    return value ? value : "";
  }
  static void toJson(const String& source, JsonVariant destination) {
    destination.set(static_cast<const std::string&>(source));
  }
  static bool checkJson(JsonVariantConst source) { return source.is<const char*>(); }
};
}