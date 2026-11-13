// Pacific time without the C library's TZ state, so core stays pure and testable (PRD §5 key rule, §7.4).
#pragma once
#include <cstdint>
#include <string>

namespace core {

struct LocalTime { int y, mo, d, h, mi, s, wday; };   // wday: 0 = Sunday

int64_t daysFromCivil(int y, int m, int d);
void civilFromDays(int64_t z, int& y, int& m, int& d);

int ptOffsetSec(int64_t utc);                       // -28800 (PST) or -25200 (PDT)
LocalTime toLocal(int64_t utc);
int64_t fromLocal(int y, int mo, int d, int h, int mi, int s = 0);   // Pacific wall clock -> UTC
int64_t localMidnight(int64_t utc);                 // UTC of 00:00 local on the same local day
std::string localDate(int64_t utc);                 // "2026-11-12"

// "2026-11-11T22:48:00-08:00", "...Z", "2026-11-12T00:15" (no offset = Pacific local), "2026-11-12 05:12" (CO-OPS lst_ldt)
bool parseTime(const std::string& s, int64_t& utc);

std::string hhmm(int64_t utc);                      // "06:40"
std::string weekdayZh(int wday);                    // "周四"

}  // namespace core
