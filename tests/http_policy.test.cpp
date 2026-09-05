#include <cassert>
#include <iostream>
#include <cstdint>
uint32_t nowTick = 0;
uint32_t millis() { return nowTick; }
void delay(uint32_t duration) { nowTick += duration; }
#include "../sms_forwarder_esp32s3_sim7670g/src/http_policy.h"
#include "../sms_forwarder_esp32s3_sim7670g/src/http_limits.h"

class FakeClient : public Stream {
public:
  bool stopped = false;
  int available() override { return 1; }
  virtual uint8_t connected() { return !stopped; }
  void stop() { stopped = true; }
  int read() override { return 65; }
  virtual int read(uint8_t* data, size_t size) { memset(data, 65, size); return size; }
  int peek() override { return 65; }
  void flush() override {}
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t*, size_t size) override { return size; }
};

int main() {
  HttpEndpoint endpoint;
  assert(parseHttpEndpoint("https://Example.com?token=secret", endpoint));
  assert(endpoint.tls && endpoint.host == "example.com" && endpoint.port == 443);
  assert(parseHttpEndpoint("http://127.0.0.1:8080/api", endpoint));
  assert(!endpoint.tls && endpoint.port == 8080);
  assert(parseHttpEndpoint("https://[::1]:8443/", endpoint) && endpoint.host == "::1");
  for (const char* invalid : {"ftp://host/a", "https://user:secret@host", "https://host:0", "http://host:65536", "http://host:x", "https://", "http://host\\bad", "http://host\r\nX: x"}) {
    assert(!parseHttpEndpoint(invalid, endpoint));
  }
  BoundedHttpResponse body(4);
  const uint8_t data[] = {'a', 'b', 'c', 'd', 'e'};
  assert(body.write(data, 4) == 4 && body.complete());
  assert(body.write(data, 1) == 0 && !body.complete());
  assert(body.body() == "abcd");
  BoundedHttpClient<FakeClient> client;
  uint8_t bytes[1024];
  for (int block = 0; block < 12; block++) assert(client.read(bytes, sizeof(bytes)) == sizeof(bytes));
  assert(client.read() == -1 && client.stopped && client.limitExceeded());
  nowTick = UINT32_MAX - 5000;
  BoundedHttpClient<FakeClient> wrapping;
  nowTick += 9999;
  assert(wrapping.connected());
  nowTick += 1;
  assert(!wrapping.connected() && wrapping.stopped && wrapping.limitExceeded());
  std::cout << "HTTP URL, response-size and deadline tests passed.\n";
}