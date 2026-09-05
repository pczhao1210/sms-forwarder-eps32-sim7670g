#ifndef HTTP_POLICY_H
#define HTTP_POLICY_H

#include <cstdint>
#include <string>

struct HttpEndpoint {
  bool tls = false;
  std::string host;
  uint16_t port = 80;
};

inline bool parseHttpEndpoint(const std::string& url, HttpEndpoint& endpoint) {
  size_t start = 0;
  if (url.compare(0, 8, "https://") == 0) {
    endpoint.tls = true;
    endpoint.port = 443;
    start = 8;
  } else if (url.compare(0, 7, "http://") == 0) {
    endpoint.tls = false;
    endpoint.port = 80;
    start = 7;
  } else {
    return false;
  }
  for (unsigned char value : url) {
    if (value <= 32 || value == 127) return false;
  }
  size_t end = url.find_first_of("/?#", start);
  std::string authority = url.substr(start, end - start);
  if (authority.empty() || authority.find('@') != std::string::npos) return false;
  size_t portStart = std::string::npos;
  if (authority[0] == '[') {
    size_t bracket = authority.find(']');
    if (bracket == std::string::npos || bracket <= 1) return false;
    endpoint.host = authority.substr(1, bracket - 1);
    if (bracket + 1 < authority.size()) {
      if (authority[bracket + 1] != ':') return false;
      portStart = bracket + 2;
    }
    for (char value : endpoint.host) {
      if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') ||
            (value >= 'A' && value <= 'F') || value == ':' || value == '.')) return false;
    }
  } else {
    size_t colon = authority.find(':');
    endpoint.host = authority.substr(0, colon);
    if (colon != std::string::npos) portStart = colon + 1;
    for (char value : endpoint.host) {
      if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') || value == '-' || value == '.')) return false;
    }
  }
  if (endpoint.host.empty() || endpoint.host.size() > 253) return false;
  for (char& value : endpoint.host) {
    if (value >= 'A' && value <= 'Z') value += 'a' - 'A';
  }
  if (portStart != std::string::npos) {
    if (portStart >= authority.size()) return false;
    uint32_t port = 0;
    for (size_t index = portStart; index < authority.size(); index++) {
      char value = authority[index];
      if (value < '0' || value > '9') return false;
      port = port * 10 + value - '0';
      if (port > 65535) return false;
    }
    if (port == 0) return false;
    endpoint.port = static_cast<uint16_t>(port);
  }
  return true;
}

#endif