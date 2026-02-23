#include "time_manager.h"
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
  if (WiFi.status() != WL_CONNECTED) {
    LOGW("TIME", "time_sync_no_wifi");
    timeSynced = false;
    return false;
  }

  LOGI("TIME", "time_sync_start");
  configTime(0, 0, "ntp.aliyun.com", "ntp.tencent.com", "cn.pool.ntp.org");

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    timeSynced = true;
    timeFallbackWarned = false;
    lastSyncSource = "ntp";
    time_t now = time(nullptr);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(now));
    LOGI("TIME", "time_sync_ok", buf);
    return true;
  }

  LOGW("TIME", "time_sync_fail");
  timeSynced = false;
  if (syncTimeFromModem()) {
    timeSynced = true;
    timeFallbackWarned = false;
    return true;
  }
  return false;
}

uint64_t getEpochMillis() {
  if (!isTimeSynced()) return 0;
  struct timeval tv;
  if (gettimeofday(&tv, nullptr) != 0) return 0;
  return static_cast<uint64_t>(tv.tv_sec) * 1000ULL + static_cast<uint64_t>(tv.tv_usec / 1000);
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
