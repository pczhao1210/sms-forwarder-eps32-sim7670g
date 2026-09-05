#ifndef TLS_CLIENT_H
#define TLS_CLIENT_H

#include <WiFiClientSecure.h>
#include <SPIFFS.h>
#include <time.h>

extern const uint8_t smsRootBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t smsRootBundleEnd[] asm("_binary_x509_crt_bundle_end");

inline bool configureTlsClient(WiFiClientSecure& client, const String& host, const String& privateCaHost) {
  if (time(nullptr) < 1609459200) return false;
  client.setHandshakeTimeout(3);
  if (!privateCaHost.isEmpty() && host.equalsIgnoreCase(privateCaHost)) {
    File certificate = SPIFFS.open("/private-ca.pem", "r");
    if (!certificate || certificate.size() == 0 || certificate.size() > 16384) return false;
    bool loaded = client.loadCACert(certificate, certificate.size());
    certificate.close();
    return loaded;
  }
  client.setCACertBundle(smsRootBundleStart, smsRootBundleEnd - smsRootBundleStart);
  return true;
}

#endif