#include "bootstrap_credentials.h"
#include "config_manager.h"
#include <Preferences.h>
#include <esp_random.h>
#include <bootloader_random.h>

static String bootstrapWebPassword;
static String setupAPPassword;
static bool bootstrapReady = false;

static String generateBootstrapPassword() {
  uint8_t entropy[16];
  bootloader_random_enable();
  esp_fill_random(entropy, sizeof(entropy));
  bootloader_random_disable();
  const char* digits = "0123456789abcdef";
  char password[33];
  for (size_t index = 0; index < sizeof(entropy); index++) {
    password[index * 2] = digits[entropy[index] >> 4];
    password[index * 2 + 1] = digits[entropy[index] & 15];
  }
  password[32] = '\0';
  return String(password);
}

bool initBootstrapCredentials() {
  if (bootstrapReady) return true;
  Preferences preferences;
  if (!preferences.begin("sms-bootstrap", false)) return false;
  String web = preferences.getString("web", "");
  String ap = preferences.getString("ap", "");
  bool stored = true;
  if (web.length() != 32) {
    web = generateBootstrapPassword();
    stored = preferences.putString("web", web) == web.length();
  }
  if (ap.length() != 32) {
    ap = generateBootstrapPassword();
    stored = preferences.putString("ap", ap) == ap.length() && stored;
  }
  preferences.end();
  if (!stored) return false;
  bootstrapWebPassword = web;
  setupAPPassword = ap;
  bootstrapReady = true;
  return true;
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