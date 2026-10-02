#ifndef TLS_CLIENT_H
#define TLS_CLIENT_H

#include <WiFiClientSecure.h>
#include <SPIFFS.h>
#include <time.h>
#include "config_manager.h"

extern const uint8_t smsRootBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t smsRootBundleEnd[] asm("_binary_x509_crt_bundle_end");

inline bool configureTlsClient(WiFiClientSecure& client, const String& host, const Config& settings,
                               const char** failureReason = nullptr) {
  auto fail = [&](const char* reason) {
    if (failureReason) *failureReason = reason;
    return false;
  };
  if (failureReason) *failureReason = nullptr;
  if (!isValidTlsHandshakeTimeout(settings.tls.handshakeTimeoutSeconds)) return fail("tls_handshake_timeout_invalid");
  if (!tlsHandshakeFitsWatchdog(settings.tls.handshakeTimeoutSeconds, settings.watchdog.timeout)) {
    return fail("tls_timeout_exceeds_watchdog");
  }
  if (time(nullptr) < 1609459200) return fail("tls_clock_unset");
  client.setHandshakeTimeout(settings.tls.handshakeTimeoutSeconds);
  const String& privateCaHost = settings.tls.privateCaHost;
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