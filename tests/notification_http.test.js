const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const root = path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src');
const source = fs.readFileSync(path.join(root, 'notification_manager.cpp'), 'utf8');
const tls = fs.readFileSync(path.join(root, 'tls_client.h'), 'utf8');
const jsonHeader = process.env.ARDUINOJSON_HEADER ||
  path.join(os.homedir(), 'Arduino/libraries/ArduinoJson/src/ArduinoJson.h');
runCpp(`
#include <cassert>
#include <cctype>
#include <climits>
#include <ctime>
#include <iostream>
#include <ArduinoJson.h>
#include "${path.join(root, 'notification_protocol.h')}"
#include "${path.join(root, 'http_policy.h')}"

class HttpString : public String {
public:
  using String::String;
  HttpString(const String& value) : String(value) {}
  bool equalsIgnoreCase(const HttpString& other) const {
    if (size() != other.size()) return false;
    for (size_t i = 0; i < size(); ++i) {
      if (std::tolower(static_cast<unsigned char>((*this)[i])) !=
          std::tolower(static_cast<unsigned char>(other[i]))) return false;
    }
    return true;
  }
};
#define String HttpString
uint32_t nowTick = 0;
uint32_t millis() { return nowTick; }
void delay(uint32_t duration) { nowTick += duration; }
#include "${path.join(root, 'http_limits.h')}"

// Core 3.3.10 HTTP errors. HTTPClient collapses every failed connect to -1.
constexpr int HTTPC_ERROR_CONNECTION_REFUSED = -1;
constexpr int HTTPC_ERROR_SEND_HEADER_FAILED = -2;
constexpr int HTTPC_ERROR_SEND_PAYLOAD_FAILED = -3;
constexpr int HTTPC_ERROR_NOT_CONNECTED = -4;
constexpr int HTTPC_ERROR_CONNECTION_LOST = -5;
constexpr int HTTPC_ERROR_NO_STREAM = -6;
constexpr int HTTPC_ERROR_NO_HTTP_SERVER = -7;
constexpr int HTTPC_ERROR_TOO_LESS_RAM = -8;
constexpr int HTTPC_ERROR_ENCODING = -9;
constexpr int HTTPC_ERROR_STREAM_WRITE = -10;
constexpr int HTTPC_ERROR_READ_TIMEOUT = -11;
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS = 0;
enum class Probe { None, Deadline, WireLimit, IdleTimeout };
struct Scenario {
  int status = 200;
  int declaredSize = -2;
  int readCode = INT_MAX;
  int tlsCode = 0;
  bool beginOk = true;
  bool caLoadOk = true;
  bool generatingData = false;
  std::string body = R"({"code":200})";
  Probe requestProbe = Probe::None;
  Probe bodyProbe = Probe::None;
  unsigned requestDelay = 17;
} scenario;
int beginCalls = 0, readCalls = 0, endCalls = 0, requests = 0, feeds = 0;
bool posted = false, bundleUsed = false, privateCaUsed = false;
unsigned handshakeTimeout = 0;
time_t wallTime = 1700000000;
time_t fakeTime(time_t*) { return wallTime; }
String logged;
std::string reasonLogged;
void logHttp(const char* tag, const char* key, const char* code, const char* reason, const char* details) {
  assert(std::string(tag) == "HTTP" && std::string(key) == "http_error");
  assert(logged.empty());
  reasonLogged = reason;
  logged = std::string("code=") + code + " err=" + reason + " detail=" + details;
}
#define LOGE(...) logHttp(__VA_ARGS__)
struct { void feedWatchdog() { ++feeds; } } watchdogManager;
struct Config { struct { String privateCaHost; } tls; } config;

class NetworkClient : public Stream {
public:
  bool stopped = false;
  int available() override { return scenario.generatingData && !stopped ? 1024 : 0; }
  virtual uint8_t connected() { return !stopped; }
  virtual void stop() { stopped = true; }
  int read() override { uint8_t value; return read(&value, 1) == 1 ? value : -1; }
  virtual int read(uint8_t* data, size_t size) {
    if (stopped || !scenario.generatingData) return -1;
    memset(data, 'x', size);
    return static_cast<int>(size);
  }
  int peek() override { return -1; }
  void flush() override {}
  size_t write(uint8_t) override { return stopped ? 0 : 1; }
  size_t write(const uint8_t*, size_t size) override { return stopped ? 0 : size; }
};
class WiFiClientSecure : public NetworkClient {
public:
  void setHandshakeTimeout(unsigned seconds) { handshakeTimeout = seconds; }
  bool loadCACert(File&, size_t size) {
    assert(size > 0 && size <= 16384);
    privateCaUsed = true;
    return scenario.caLoadOk;
  }
  void setCACertBundle(const uint8_t*, size_t) { bundleUsed = true; }
  int lastError(char* buffer, size_t size) {
    snprintf(buffer, size, "%s", "X509 - Certificate verification failed");
    return scenario.tlsCode;
  }
};
const uint8_t rootBundle[] = {1, 2};
const uint8_t* smsRootBundleStart = rootBundle;
const uint8_t* smsRootBundleEnd = rootBundle + sizeof(rootBundle);
#define time fakeTime
${extractFunction(tls, 'inline bool configureTlsClient(')}
#undef time

void probe(NetworkClient& client, Probe action) {
  if (action == Probe::Deadline) {
    nowTick += 10000;
    assert(!client.connected());
  } else if (action == Probe::WireLimit) {
    scenario.generatingData = true;
    uint8_t bytes[1024];
    for (int block = 0; block < 12; ++block) assert(client.read(bytes, sizeof(bytes)) == 1024);
    assert(client.available() == 0);
    scenario.generatingData = false;
  } else if (action == Probe::IdleTimeout) {
    char byte;
    assert(client.readBytes(&byte, 1) == 0);
  }
}
class HTTPClient {
public:
  NetworkClient* client = nullptr;
  bool begin(NetworkClient& selected, const String&) {
    ++beginCalls;
    client = &selected;
    return scenario.beginOk;
  }
  void addHeader(const char* key, const String&) { assert(std::string(key) == "Content-Type"); }
  void setConnectTimeout(int timeout) { assert(timeout == 2000); }
  void setTimeout(int timeout) { assert(timeout == 2000); }
  void setFollowRedirects(int redirects) { assert(redirects == HTTPC_DISABLE_FOLLOW_REDIRECTS); }
  int GET() {
    ++requests;
    nowTick += scenario.requestDelay;
    probe(*client, scenario.requestProbe);
    return scenario.status;
  }
  int POST(const String&) { posted = true; return GET(); }
  int getSize() const { return scenario.declaredSize == -2 ? static_cast<int>(scenario.body.size()) : scenario.declaredSize; }
  int writeToStream(Stream* response) {
    ++readCalls;
    nowTick += 3;
    probe(*client, scenario.bodyProbe);
    size_t written = response->write(reinterpret_cast<const uint8_t*>(scenario.body.data()), scenario.body.size());
    if (written != scenario.body.size()) return HTTPC_ERROR_STREAM_WRITE;
    return scenario.readCode == INT_MAX ? static_cast<int>(written) : scenario.readCode;
  }
  void end() {
    ++endCalls;
    scenario.tlsCode = 0;
    client->stop();
  }
  static String errorToString(int code) {
    switch (code) {
      case -1: return "connection refused";
      case -2: return "send header failed";
      case -3: return "send payload failed";
      case -4: return "not connected";
      case -5: return "connection lost";
      case -6: return "no stream";
      case -7: return "no HTTP server";
      case -8: return "too less ram";
      case -9: return "Transfer-Encoding not supported";
      case -10: return "Stream write error";
      case -11: return "read Timeout";
      default: return "";
    }
  }
};
struct NotificationManager {
  static bool sendHTTPRequest(const String&, const String&, const String&, NotificationProvider, const Config&);
};
${extractFunction(source, 'static const char* notificationProviderName(')}
${extractFunction(source, 'static void appendNotificationProviderStatus(')}
${extractFunction(source, 'bool NotificationManager::sendHTTPRequest(')}

void reset() {
  scenario = {};
  nowTick = 0;
  wallTime = 1700000000;
  beginCalls = readCalls = endCalls = requests = feeds = 0;
  posted = bundleUsed = privateCaUsed = false;
  handshakeTimeout = 0;
  logged.clear();
  reasonLogged.clear();
  config = {};
  SPIFFS.files.clear();
  FakeFS::failOpen = false;
}
void contains(const std::string& value) {
  if (logged.find(value) == std::string::npos) {
    std::cerr << "Missing " << value << " in " << logged << "\\n";
    assert(false);
  }
}
bool send(NotificationProvider provider = NotificationProvider::Bark, bool tls = true, const String& payload = "") {
  bool result = NotificationManager::sendHTTPRequest(
    tls ? "https://example.invalid/url-secret?token=credential-secret" : "http://example.invalid/url-secret",
    payload, "application/json", provider, config);
  for (const char* secret : {"url-secret", "credential-secret", "sms-secret", "body-secret", "example.invalid"}) {
    assert(logged.find(secret) == std::string::npos);
  }
  return result;
}
void fails(const char* reason, NotificationProvider provider = NotificationProvider::Bark, bool tls = true) {
  assert(!send(provider, tls));
  if (reasonLogged != reason) {
    std::cerr << "Expected " << reason << ", got " << logged << "\\n";
    assert(false);
  }
}

int main() {
  reset();
  assert(send(NotificationProvider::Bark, true, "sms-secret"));
  assert(posted && logged.empty() && readCalls == 1 && endCalls == 1);
  assert(bundleUsed && !privateCaUsed && handshakeTimeout == 3);

  // No status means no response body: even a bogus large length must not mask it.
  for (bool tls : {false, true}) {
    for (int code = -11; code <= 0; ++code) {
      reset();
      scenario.status = code;
      scenario.declaredSize = 50000;
      scenario.body = "body-secret";
      assert(!send(NotificationProvider::Bark, tls));
      assert(readCalls == 0 && endCalls == 1);
      assert(logged.find("too_large") == std::string::npos);
      assert(logged.find("response_incomplete") == std::string::npos);
      contains("code=" + std::to_string(code));
      contains("elapsed_ms=17");
      contains(tls ? "transport=https" : "transport=http");
      contains("provider=bark");
      if (code == -1) {
        assert(reasonLogged == "connect_failed");
        contains("phase=connect");
        contains("http_error=connection refused");
      } else if (code == -11) assert(reasonLogged == "response_header_timeout");
      else if (code == -2 || code == -3) assert(reasonLogged == "request_write_failed");
    }
  }
  reset();
  scenario.status = -1;
  scenario.tlsCode = -9984;
  fails("tls_connect_failed");
  contains("tls_code=-9984");
  contains("tls_error=X509 - Certificate verification failed");
  reset();
  scenario.status = -1;
  scenario.tlsCode = -1;
  fails("connect_failed");
  contains("tls_error=connect_or_handshake_failed");

  reset();
  scenario.beginOk = false;
  fails("http_begin_failed");
  assert(requests == 0 && readCalls == 0 && endCalls == 0);
  reset();
  assert(!NotificationManager::sendHTTPRequest("https://user:credential-secret@host/sms-secret", "", "",
                                              NotificationProvider::Bark, config));
  assert(reasonLogged == "invalid_url" && beginCalls == 0 && logged.find("secret") == std::string::npos);
  reset();
  wallTime = 0;
  fails("tls_clock_unset");
  assert(beginCalls == 0 && !bundleUsed && !privateCaUsed);
  assert(send(NotificationProvider::Custom, false)); // Plain HTTP has no TLS clock dependency.
  for (const auto& entry : {std::make_pair(-1, "tls_private_ca_unavailable"),
                           std::make_pair(0, "tls_private_ca_empty"),
                           std::make_pair(16385, "tls_private_ca_too_large"),
                           std::make_pair(16384, "tls_private_ca_load_failed")}) {
    reset();
    config.tls.privateCaHost = "EXAMPLE.INVALID";
    if (entry.first >= 0) SPIFFS.files["/private-ca.pem"] = std::make_shared<std::string>(entry.first, 'x');
    scenario.caLoadOk = false;
    fails(entry.second);
    assert(beginCalls == 0 && !bundleUsed);
  }
  reset();
  config.tls.privateCaHost = "EXAMPLE.INVALID";
  SPIFFS.files["/private-ca.pem"] = std::make_shared<std::string>(16384, 'x');
  assert(send());
  assert(privateCaUsed && !bundleUsed);
  reset();
  config.tls.privateCaHost = "different.invalid";
  assert(send());
  assert(bundleUsed && !privateCaUsed);
  WiFiClientSecure compatible;
  assert(configureTlsClient(compatible, "example.invalid", "")); // Existing three-argument callers.

  reset();
  scenario.declaredSize = 4097;
  fails("response_body_too_large");
  contains("declared_bytes=4097");
  contains("read=not_attempted");
  assert(readCalls == 0);
  reset();
  scenario.declaredSize = -1;
  scenario.body.assign(4097, 'x');
  fails("response_body_too_large");
  contains("read_code=-10");
  reset();
  scenario.body.assign(4096, 'x');
  assert(send(NotificationProvider::Custom));
  reset();
  scenario.declaredSize = 40;
  fails("response_incomplete");
  contains("body_bytes=12");
  reset();
  scenario.readCode = -5;
  fails("response_read_failed");
  contains("read_code=-5");
  contains("read_error=connection lost");
  reset();
  scenario.readCode = -11;
  fails("response_read_timeout");
  reset();
  scenario.readCode = -10;
  scenario.bodyProbe = Probe::IdleTimeout;
  fails("response_read_timeout");
  contains("read_idle_timeout=1");
  contains("elapsed_ms=2020");
  reset();
  scenario.bodyProbe = Probe::IdleTimeout;
  assert(send()); // A recovered short read must not turn eventual success into failure.
  for (bool beforeHeaders : {false, true}) {
    for (Probe action : {Probe::Deadline, Probe::WireLimit}) {
      reset();
      if (beforeHeaders) {
        scenario.status = -11;
        scenario.requestProbe = action;
      } else scenario.bodyProbe = action;
      fails(action == Probe::Deadline ? "request_deadline_exceeded" : "response_receive_limit");
      if (action == Probe::WireLimit) contains("received_bytes=12288");
      assert(feeds > 0);
    }
  }
  reset();
  nowTick = UINT32_MAX - 5;
  scenario.status = -1;
  fails("connect_failed");
  contains("elapsed_ms=17");
  reset();
  nowTick = UINT32_MAX - 5000;
  BoundedHttpClient<NetworkClient> wrapping;
  nowTick += 9999;
  assert(wrapping.connected());
  nowTick += 1;
  assert(!wrapping.connected() && wrapping.limitExceeded() && wrapping.deadlineExceeded());
  assert(!wrapping.receiveLimitExceeded());
  nowTick += 1;
  assert(!wrapping.connected() && wrapping.deadlineExceeded());

  struct Response { NotificationProvider provider; const char* body; bool success; };
  const Response responses[] = {
    {NotificationProvider::Bark, R"({"code":200})", true},
    {NotificationProvider::Bark, R"({"code":400,"message":"body-secret"})", false},
    {NotificationProvider::Bark, R"({"code":"body-secret"})", false},
    {NotificationProvider::ServerChan, R"({"code":0})", true},
    {NotificationProvider::ServerChan, R"({"errno":"0"})", true},
    {NotificationProvider::ServerChan, R"({"code":40001})", false},
    {NotificationProvider::Telegram, R"({"ok":true})", true},
    {NotificationProvider::Telegram, R"({"ok":false,"error_code":429,"description":"body-secret"})", false},
    {NotificationProvider::Telegram, R"({"ok":"true"})", false},
    {NotificationProvider::DingTalk, R"({"errcode":"0"})", true},
    {NotificationProvider::DingTalk, R"({"errcode":310000})", false},
    {NotificationProvider::Feishu, R"({"code":0})", true},
    {NotificationProvider::Feishu, R"({"StatusCode":"0"})", true},
    {NotificationProvider::Feishu, R"({"code":19001})", false},
    {NotificationProvider::Custom, "", true},
    {NotificationProvider::Custom, "body-secret", true},
  };
  for (const auto& entry : responses) {
    reset();
    scenario.status = 201;
    scenario.body = entry.body;
    assert(send(entry.provider) == entry.success);
    if (!entry.success) assert(reasonLogged == "provider_rejected");
    reset();
    scenario.status = 500;
    scenario.body = entry.body;
    fails("http_rejected", entry.provider);
    contains("code=500");
  }
  reset();
  scenario.body = R"({"ok":false,"error_code":429,"description":"body-secret"})";
  fails("provider_rejected", NotificationProvider::Telegram);
  contains("ok=false error_code=429");
  reset();
  scenario.status = 302;
  scenario.body = "<html>body-secret</html>";
  fails("http_rejected");
  assert(requests == 1);
  reset();
  scenario.body = "body-secret";
  fails("invalid_json");
  contains("json_error=InvalidInput");
  reset();
  scenario.body = "";
  fails("invalid_json");
  contains("json_error=EmptyInput");
  reset();
  scenario.status = 204;
  scenario.body = "";
  assert(send(NotificationProvider::Custom));
#if ARDUINOJSON_VERSION_MAJOR == 6
  reset();
  scenario.body = std::string("{\\"a\\":\\"") + std::string(4088, 'x') + "\\"}";
  fails("invalid_json");
  contains("json_error=NoMemory");
#endif
  std::cout << "Production HTTP/TLS diagnostic, limit, redaction and success tests passed.\\n";
}
`, ['-DARDUINOJSON_HEADER="' + jsonHeader + '"', '-I', path.dirname(jsonHeader)]);
