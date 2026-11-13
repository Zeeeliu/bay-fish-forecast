#include "tz.h"
#include <cstdio>
#include <cstdlib>

namespace core {

// Howard Hinnant's civil-date algorithms.
int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

void civilFromDays(int64_t z, int& y, int& m, int& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  y = (int)(yoe + era * 400 + (m <= 2));
}

static int weekdayOfDays(int64_t z) { return (int)((z % 7 + 11) % 7); }   // 1970-01-01 was a Thursday (4)

static int64_t nthSunday(int y, int m, int n) {   // day number of the n-th Sunday of the month
  int64_t first = daysFromCivil(y, m, 1);
  int wd = weekdayOfDays(first);
  return first + (7 - wd) % 7 + 7 * (n - 1);
}

int ptOffsetSec(int64_t utc) {
  int y, m, d;
  civilFromDays(utc / 86400 - (utc < 0 && utc % 86400), y, m, d);
  // DST: from 2:00 PST on the 2nd Sunday of March (10:00 UTC) to 2:00 PDT on the 1st Sunday of November (09:00 UTC)
  int64_t start = nthSunday(y, 3, 2) * 86400 + 10 * 3600;
  int64_t end = nthSunday(y, 11, 1) * 86400 + 9 * 3600;
  return (utc >= start && utc < end) ? -7 * 3600 : -8 * 3600;
}

LocalTime toLocal(int64_t utc) {
  int64_t t = utc + ptOffsetSec(utc);
  int64_t days = t >= 0 ? t / 86400 : (t - 86399) / 86400;
  int64_t sec = t - days * 86400;
  LocalTime lt{};
  civilFromDays(days, lt.y, lt.mo, lt.d);
  lt.h = (int)(sec / 3600); lt.mi = (int)(sec % 3600 / 60); lt.s = (int)(sec % 60);
  lt.wday = weekdayOfDays(days);
  return lt;
}

int64_t fromLocal(int y, int mo, int d, int h, int mi, int s) {
  int64_t wall = daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
  int64_t t = wall + 8 * 3600;                 // try PST
  if (ptOffsetSec(t) == -8 * 3600) return t;
  return wall + 7 * 3600;                      // PDT
}

int64_t localMidnight(int64_t utc) {
  LocalTime lt = toLocal(utc);
  return fromLocal(lt.y, lt.mo, lt.d, 0, 0, 0);
}

std::string localDate(int64_t utc) {
  LocalTime lt = toLocal(utc);
  char b[16]; snprintf(b, sizeof b, "%04d-%02d-%02d", lt.y, lt.mo, lt.d);
  return b;
}

bool parseTime(const std::string& s, int64_t& utc) {
  int y, mo, d, h = 0, mi = 0, sec = 0;
  if (s.size() < 10 || sscanf(s.c_str(), "%4d-%2d-%2d", &y, &mo, &d) != 3) return false;
  size_t p = 10;
  if (p < s.size() && (s[p] == 'T' || s[p] == ' ')) {
    int n = sscanf(s.c_str() + p + 1, "%2d:%2d:%2d", &h, &mi, &sec);
    if (n < 2) return false;
    if (n < 3) sec = 0;
    p += 1 + (n == 3 ? 8 : 5);
    while (p < s.size() && (s[p] == '.' || (s[p] >= '0' && s[p] <= '9'))) p++;   // fractional seconds
  }
  if (p < s.size() && (s[p] == 'Z' || s[p] == 'z')) { utc = daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + sec; return true; }
  if (p < s.size() && (s[p] == '+' || s[p] == '-')) {
    int oh = 0, om = 0;
    if (sscanf(s.c_str() + p + 1, "%2d:%2d", &oh, &om) < 1) return false;
    int off = (oh * 3600 + om * 60) * (s[p] == '-' ? -1 : 1);
    utc = daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + sec - off;
    return true;
  }
  utc = fromLocal(y, mo, d, h, mi, sec);
  return true;
}

std::string hhmm(int64_t utc) {
  LocalTime lt = toLocal(utc);
  char b[8]; snprintf(b, sizeof b, "%02d:%02d", lt.h, lt.mi);
  return b;
}

std::string weekdayZh(int wday) {
  static const char* W[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
  return W[((wday % 7) + 7) % 7];
}

}  // namespace core
