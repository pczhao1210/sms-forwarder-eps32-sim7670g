#ifndef TLS_CLIENT_H
#define TLS_CLIENT_H

#include <WiFiClientSecure.h>
#include <SPIFFS.h>
#include <time.h>

extern const uint8_t smsRootBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t smsRootBundleEnd[] asm("_binary_x509_crt_bundle_end");

inline bool configureTlsClient(WiFiClientSecure& client, const String& host, const String& privateCaHost,
                               const char** failureReason = nullptr) {
  auto fail = [&](const char* reason) {
    if (failureReason) *failureReason = reason;
    return false;
  };
  if (failureReason) *failureReason = nullptr;
  if (time(nullptr) < 1609459200) return fail("tls_clock_unset");
  client.setHandshakeTimeout(3);
  if (!privateCaHost.isEmpty() && host.equalsIgnoreCase(privateCaHost)) {
    File certificate = SPIFFS.open("/private-ca.pem", "r");
    if (!certificate) return fail("tls_private_ca_unavailable");
    if (certificate.size() == 0) return fail("tls_private_ca_empty");
    if (certificate.size() > 16384) return fail("tls_private_ca_too_large");
    bool loaded = client.loadCACert(certificate, certificate.size());
    certificate.close();
    return loaded ? true : fail("tls_private_ca_load_failed");
  }
  client.setCACertBundle(smsRootBundleStart, smsRootBundleEnd - smsRootBundleStart);
  return true;
}

#endif