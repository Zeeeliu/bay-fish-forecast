// Chinese lookup tables and formatting helpers (PRD §7.1.6, §7.3, §7.4).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace core {

struct EventInfo { const char* event; int priority; const char* nameZh; const char* shortTpl; const char* fullTpl; const char* shortEn; };
const EventInfo* eventInfo(const std::string& event);     // nullptr when unmapped (priority 40)
std::vector<std::string> eventNames();

std::string zoneZh(const std::string& zone);               // "" when unknown
std::string weatherZh(int code);
std::string weatherIcon(int code);                         // sun | partly | cloud | fog | rain | storm
std::string windDirZh(double deg);
std::string fmt1(double v);                                // 1 decimal, U+2212 for negatives
std::string fmt2(double v);
std::string fmtInt(double v);
std::string dayLabel(int64_t dayUtc, int64_t now);         // 今天 / 周五
std::string mdText(int64_t utc);                           // 11/13
std::string weekdayTime(int64_t utc);                      // 周五 18:00
std::string dateHeader(int64_t utc);                       // 11月12日 周四
// English (landing page option)
std::string weatherEn(int code);
std::string windDirEn(double deg);
std::string weekdayEn(int wday);
std::string weekdayTimeEn(int64_t utc);                    // Fri 18:00
std::string dateHeaderEn(int64_t utc);                     // Thu, Nov 12

}  // namespace core
