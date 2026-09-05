#include "bootstrap_credentials.h"
#include "config_manager.h"
#include <Preferences.h>

static const String bootstrapWebPassword = "admin1234";
static const String setupAPPassword = "12345678";
static String legacyBootstrapWebPassword;
static bool bootstrapReady = false;

bool initBootstrapCredentials() {
  if (bootstrapReady) return true;
  legacyBootstrapWebPassword = "";
  Preferences preferences;
  if (preferences.begin("sms-bootstrap", true)) {
    legacyBootstrapWebPassword = preferences.getString("web", "");
    preferences.end();
  }
  if (legacyBootstrapWebPassword.length() != 32) legacyBootstrapWebPassword = "";
  bootstrapReady = true;
  return true;
}

bool needsBootstrapWebAuth(const String& password) {
  return password.isEmpty() ||
         (!legacyBootstrapWebPassword.isEmpty() && password == legacyBootstrapWebPassword);
}

const String& getBootstrapWebPassword() { return bootstrapWebPassword; }
const String& getSetupAPPassword() { return setupAPPassword; }

void printBootstrapAccess() {
  if (bootstrapReady && config.webAuth.password == bootstrapWebPassword) {
    Serial.print("[SETUP] Web username: ");
    Serial.println(config.webAuth.username);
    Serial.print("[SETUP] Web password: ");
    Serial.println(bootstrapWebPassword);
  }
}

void pollBootstrapRecovery() {
  static String command;
  static bool overflow = false;
  for (int count = 0; count < 128 && Serial.available(); count++) {
    char value = static_cast<char>(Serial.read());
    if (value == '\r') continue;
    if (value != '\n') {
      if (command.length() < 32) command += value;
      else overflow = true;
      continue;
    }
    if (!overflow && command == "RESET WEB AUTH" && bootstrapReady) {
      auto previous = config.webAuth;
      config.webAuth.enabled = true;
      config.webAuth.username = "admin";
      config.webAuth.password = bootstrapWebPassword;
      if (saveConfig()) printBootstrapAccess();
      else {
        config.webAuth = previous;
        Serial.println("[SETUP] Recovery save failed; credentials unchanged.");
      }
    }
    command = "";
    overflow = false;
  }
}