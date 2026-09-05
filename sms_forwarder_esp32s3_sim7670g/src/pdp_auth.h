#ifndef PDP_AUTH_H
#define PDP_AUTH_H

#include <string>
#include "input_validation.h"

inline bool buildPdpAuthCommand(const std::string& user, const std::string& password, std::string& command) {
  if (user.size() > 64 || password.size() > 64 ||
      !isSafeAtField(user.data(), user.size()) || !isSafeAtField(password.data(), password.size())) return false;
  if (user.empty() && password.empty()) {
    command = "AT+CGAUTH=1,0";
    return true;
  }
  if (user.empty()) return false;
  command = "AT+CGAUTH=1,1,\"" + password + "\",\"" + user + "\"";
  return true;
}

inline std::string redactPdpCredentials(const std::string& text) {
  std::string upper = text;
  for (char& value : upper) {
    if (value >= 'a' && value <= 'z') value -= 'a' - 'A';
  }
  if (upper.find("+CGAUTH") != std::string::npos) return "[PDP credentials redacted]";
  return text;
}

#endif