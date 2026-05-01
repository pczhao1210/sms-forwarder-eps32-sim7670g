#include "time_manager.h"
#include "config_manager.h"
#include "log_manager.h"
#include "i18n.h"
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include "sim7670g_manager.h"

static bool timeSynced = false;
static bool timeFallbackWarned = false;
static String lastSyncSource = "none";
static const time_t MIN_VALID_EPOCH = 1609459200; // 2021-01-01
static unsigned long lastTimeSyncAttemptMs = 0;
static unsigned long lastTimeSyncSuccessMs = 0;
static unsigned long lastModemSyncAttemptMs = 0;
static const unsigned long TIME_SYNC_RETRY_INTERVAL_MS = 300000UL;
static const unsigned long TIME_SYNC_REFRESH_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
static const unsigned long MODEM_TIME_SYNC_RETRY_INTERVAL_MS = 120000UL;
static bool ntpSyncInProgress = false;
static unsigned long ntpSyncStartMs = 0;
static const unsigned long NTP_SYNC_TIMEOUT_MS = 10000UL;

static void markNtpSyncSuccess() {
  timeSynced = true;
  timeFallbackWarned = false;
  lastSyncSource = "ntp";
  lastTimeSyncSuccessMs = millis();
  ntpSyncInProgress = false;
  time_t now = time(nullptr);
  char buf[16];
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(now));
  LOGI("TIME", "time_sync_ok", buf);
}

static bool startNtpSyncAttempt() {
  lastTimeSyncAttemptMs = millis();
  if (WiFi.status() != WL_CONNECTED) {
    LOGW("TIME", "time_sync_no_wifi");
    timeSynced = false;
    ntpSyncInProgress = false;
    return false;
  }

  LOGI("TIME", "time_sync_start");
  configTime(0, 0, "ntp.aliyun.com", "ntp.tencent.com", "cn.pool.ntp.org");
  ntpSyncInProgress = true;
  ntpSyncStartMs = millis();
  return true;
}

static time_t timegmCompat(struct tm* t) {
#ifdef timegm
  return timegm(t);
#else
  const char* oldTz = getenv("TZ");
  setenv("TZ", "UTC0", 1);
  tzset();
  time_t result = mktime(t);
  if (oldTz) {
    setenv("TZ", oldTz, 1);
  } else {
    unsetenv("TZ");
  }
  tzset();
  return result;
#endif
}

static bool applyEpoch(time_t epoch) {
  if (epoch <= 0) return false;
  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  return settimeofday(&tv, nullptr) == 0;
}

static bool parseModemClock(const String& response, time_t& epochOut) {
  int idx = response.indexOf("+CCLK:");
  if (idx < 0) return false;
  int quote1 = response.indexOf('"', idx);
  int quote2 = response.indexOf('"', quote1 + 1);
  if (quote1 < 0 || quote2 < 0) return false;
  String ts = response.substring(quote1 + 1, quote2);
  if (ts.length() < 17) return false;

  int year = ts.substring(0, 2).toInt() + 2000;
  int month = ts.substring(3, 5).toInt();
  int day = ts.substring(6, 8).toInt();
  int hour = ts.substring(9, 11).toInt();
  int minute = ts.substring(12, 14).toInt();
  int second = ts.substring(15, 17).toInt();

  int tzSignPos = ts.lastIndexOf('+');
  int tzSign = 1;
  if (tzSignPos < 0) {
    tzSignPos = ts.lastIndexOf('-');
    tzSign = -1;
  }
  int tzQuarter = 0;
  if (tzSignPos >= 0 && tzSignPos + 1 < ts.length()) {
    tzQuarter = ts.substring(tzSignPos + 1).toInt() * tzSign;
  }

  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;

  time_t epoch = timegmCompat(&t);
  if (epoch <= 0) return false;
  epoch -= static_cast<time_t>(tzQuarter) * 15 * 60;
  epochOut = epoch;
  return true;
}

bool syncTimeFromModem() {
  lastModemSyncAttemptMs = millis();
  ntpSyncInProgress = false;
  if (simState != SIM_STATE_READY) {
    LOGW("TIME", "time_sync_modem_not_ready");
    return false;
  }
  LOGI("TIME", "time_sync_modem_start");
  String resp = sendATCommand("AT+CCLK?");
  if (resp.indexOf("BUSY") >= 0) {
    LOGW("TIME", "time_sync_modem_busy");
    return false;
  }
  time_t epoch = 0;
  if (!parseModemClock(resp, epoch)) {
    LOGW("TIME", "time_sync_modem_fail");
    return false;
  }
  if (applyEpoch(epoch)) {
    timeSynced = true;
    timeFallbackWarned = false;
    lastSyncSource = "modem";
    lastTimeSyncSuccessMs = millis();
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(epoch));
    LOGI("TIME", "time_sync_modem_ok", buf);
    return true;
  }
  LOGW("TIME", "time_sync_modem_fail");
  return false;
}

bool isTimeSynced() {
  time_t now = time(nullptr);
  return now >= MIN_VALID_EPOCH;
}

bool initTimeSync() {
  if (!startNtpSyncAttempt()) {
    return false;
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    markNtpSyncSuccess();
    return true;
  }

  LOGW("TIME", "time_sync_fail");
  timeSynced = false;
  ntpSyncInProgress = false;
  if (syncTimeFromModem()) {
    timeSynced = true;
    timeFallbackWarned = false;
    return true;
  }
  return false;
}

void pollTimeSyncRecovery() {
  static bool lastWifiConnected = false;
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  unsigned long now = millis();

  if (ntpSyncInProgress) {
    if (!wifiConnected) {
      ntpSyncInProgress = false;
    } else {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 1)) {
        markNtpSyncSuccess();
      } else if (now - ntpSyncStartMs >= NTP_SYNC_TIMEOUT_MS) {
        LOGW("TIME", "time_sync_fail");
        ntpSyncInProgress = false;
        timeSynced = false;
        if (simState == SIM_STATE_READY) {
          syncTimeFromModem();
        }
      }
    }
  }

  bool wifiJustConnected = wifiConnected && !lastWifiConnected;
  bool timeValid = isTimeSynced();
  bool dueForRefresh = timeValid && lastSyncSource == "ntp" &&
                       lastTimeSyncSuccessMs > 0 &&
                       (now - lastTimeSyncSuccessMs >= TIME_SYNC_REFRESH_INTERVAL_MS);
  bool dueForRetry = !timeValid &&
                     (lastTimeSyncAttemptMs == 0 || (now - lastTimeSyncAttemptMs >= TIME_SYNC_RETRY_INTERVAL_MS));

  if (wifiConnected && (wifiJustConnected || dueForRetry || dueForRefresh)) {
    if (!ntpSyncInProgress) {
      startNtpSyncAttempt();
    }
  } else if (!wifiConnected && !timeValid && simState == SIM_STATE_READY) {
    if (lastModemSyncAttemptMs == 0 || (now - lastModemSyncAttemptMs >= MODEM_TIME_SYNC_RETRY_INTERVAL_MS)) {
      syncTimeFromModem();
    }
  }

  lastWifiConnected = wifiConnected;
}

uint64_t getEpochMillis() {
  if (!isTimeSynced()) return 0;
  struct timeval tv;
  if (gettimeofday(&tv, nullptr) != 0) return 0;
  return static_cast<uint64_t>(tv.tv_sec) * 1000ULL + static_cast<uint64_t>(tv.tv_usec / 1000);
}

int getConfiguredTimezoneOffsetMinutes() {
  return config.time.timezoneOffsetMinutes;
}

bool getConfiguredLocalTime(struct tm& timeinfo) {
  if (!isTimeSynced()) return false;
  time_t now = time(nullptr);
  if (now <= 0) return false;
  now += static_cast<time_t>(getConfiguredTimezoneOffsetMinutes()) * 60;
  gmtime_r(&now, &timeinfo);
  return true;
}

int getConfiguredLocalMinuteOfDay() {
  struct tm timeinfo = {};
  if (!getConfiguredLocalTime(timeinfo)) return -1;
  return timeinfo.tm_hour * 60 + timeinfo.tm_min;
}

const char* getTimeSyncSource() {
  return lastSyncSource.c_str();
}

String getTimestampMsString() {
  uint64_t epochMs = getEpochMillis();
  if (epochMs == 0) {
    if (!timeFallbackWarned) {
      LOGW("TIME", "time_sync_fallback");
      timeFallbackWarned = true;
    }
    return String(millis());
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(epochMs));
  return String(buf);
}
