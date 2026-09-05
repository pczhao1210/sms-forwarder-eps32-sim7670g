#ifndef BOOTSTRAP_CREDENTIALS_H
#define BOOTSTRAP_CREDENTIALS_H

#include <Arduino.h>

bool initBootstrapCredentials();
const String& getBootstrapWebPassword();
const String& getSetupAPPassword();
bool needsBootstrapWebAuth(const String& password);
void printBootstrapAccess();
void pollBootstrapRecovery();

#endif