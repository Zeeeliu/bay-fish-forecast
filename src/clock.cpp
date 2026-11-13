// Time: RTC (RX8130) at boot, NTP when online, written back to the RTC (PRD §10.4). The RTC holds UTC.
#include <M5Unified.h>
#include <sys/time.h>
#include <time.h>
#include "app.h"

static const time_t kValidAfter = 1735689600;   // 2025-01-01

int64_t nowUtc() { return (int64_t)time(nullptr); }

void clockBegin() {
  if (!M5.Rtc.isEnabled()) { Serial.println("clock: no RTC"); return; }
  auto dt = M5.Rtc.getDateTime();
  struct tm t = {};
  t.tm_year = dt.date.year - 1900; t.tm_mon = dt.date.month - 1; t.tm_mday = dt.date.date;
  t.tm_hour = dt.time.hours; t.tm_min = dt.time.minutes; t.tm_sec = dt.time.seconds;
  setenv("TZ", "UTC0", 1); tzset();
  time_t utc = mktime(&t);
  if (utc > kValidAfter) {
    timeval tv = {utc, 0};
    settimeofday(&tv, nullptr);
    app.clockValid = true;
  }
  Serial.printf("clock: RTC %04d-%02d-%02d %02d:%02d:%02d UTC -> %s\n", dt.date.year, dt.date.month, dt.date.date,
                dt.time.hours, dt.time.minutes, dt.time.seconds, app.clockValid ? "valid" : "not set");
}

void clockOnNtp() {   // called by the net task once SNTP has set the time
  time_t now = time(nullptr);
  if (now < kValidAfter) return;
  app.clockValid = true;
  app.ntpSynced = true;
  struct tm g;
  gmtime_r(&now, &g);
  if (M5.Rtc.isEnabled()) M5.Rtc.setDateTime(&g);
  Serial.printf("clock: NTP %lld, RTC updated\n", (long long)now);
}
