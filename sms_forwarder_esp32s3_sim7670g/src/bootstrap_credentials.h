#ifndef BOOTSTRAP_CREDENTIALS_H
#define BOOTSTRAP_CREDENTIALS_H

#include <Arduino.h>

bool initBootstrapCredentials();
const String& getBootstrapWebPassword();
const String& getSetupAPPassword();
void printBootstrapAccess();
void pollBootstrapRecovery();

inline bool needsBootstrapWebAuth(const String& password) {
  return password.isEmpty() || password == "admin1234";
}

#endif