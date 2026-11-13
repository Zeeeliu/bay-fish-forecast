#include "marine.h"
#include <ArduinoJson.h>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include "tz.h"

namespace core {

bool parseNwsMarine(const std::string& body, MarineFc& out) {
  out = MarineFc{};
  JsonDocument filter;
  filter["updated"] = true;
  filter["periods"][0]["name"] = true; filter["periods"][0]["detailedForecast"] = true;
  filter["properties"]["updated"] = true;
  filter["properties"]["periods"][0]["name"] = true; filter["properties"]["periods"][0]["detailedForecast"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter)) != DeserializationError::Ok) return false;
  JsonVariantConst root = doc.as<JsonVariantConst>();   // ld+json has periods at the top; geo+json under properties
  if (!root["periods"].is<JsonArrayConst>()) root = root["properties"];
  parseTime(root["updated"] | "", out.updated);
  for (JsonObjectConst p : root["periods"].as<JsonArrayConst>()) out.periods.push_back({p["name"] | "", p["detailedForecast"] | ""});
  return out.updated && !out.periods.empty();
}

static bool isCompass(const std::string& w) {
  static const char* C[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW", "Variable"};
  for (auto c : C) if (w == c) return true;
  return false;
}
static bool isNum(const std::string& w) { return !w.empty() && isdigit((unsigned char)w[0]); }

MarineWx parseMarineText(const std::string& full) {
  MarineWx m;
  std::string text = full.substr(0, full.find("..."));   // drop "IN THE DEEP WATER CHANNEL..." / "ACROSS THE BAR..." add-ons
  // sentences
  std::vector<std::string> sentences;
  std::string cur;
  for (size_t i = 0; i < text.size(); i++) {
    cur += text[i];
    if (text[i] == '.' && (i + 1 == text.size() || text[i + 1] == ' ') && !(i > 0 && isdigit((unsigned char)text[i - 1]) && i + 1 < text.size() && isdigit((unsigned char)text[i + 1]))) {
      sentences.push_back(cur); cur.clear();
    }
  }
  if (!cur.empty()) sentences.push_back(cur);
  for (auto& s : sentences) {
    std::vector<std::string> w;
    std::istringstream in(s);
    for (std::string t; in >> t;) { while (!t.empty() && (t.back() == ',' || t.back() == '.')) t.pop_back(); w.push_back(t); }
    if (w.empty()) continue;
    bool seas = w[0] == "Seas" || w[0] == "Waves";
    bool detail = w[0] == "Wave" && w.size() > 1 && w[1] == "Detail:";
    bool windy = s.find(" kt") != std::string::npos && s.find("current") == std::string::npos;
    std::string dir;
    double bestFt = -1;
    for (size_t i = 0; i < w.size(); i++) {
      if (isCompass(w[i])) dir = w[i];
      if (!isNum(w[i])) continue;
      double a = atof(w[i].c_str()), b = a;
      size_t j = i + 1;
      if (j + 1 < w.size() && w[j] == "to" && isNum(w[j + 1])) { b = atof(w[j + 1].c_str()); j += 2; }
      std::string unit = j < w.size() ? w[j] : "";
      bool gustCtx = false;   // "with gusts up to 40 kt" / "Gusts up to 40 kt"
      for (size_t k = i >= 3 ? i - 3 : 0; k < i; k++) if (w[k] == "gusts" || w[k] == "Gusts") gustCtx = true;
      if (unit == "kt" && windy && !seas && !detail) {
        if (gustCtx) m.gust = std::max(m.gust, (int)b);
        else if ((int)b > m.windHi) { m.windHi = (int)b; m.windLo = (int)a; if (!dir.empty()) m.dir = dir; }
      } else if (unit == "ft" && seas) {
        if (m.seasLoFt < 0) m.seasLoFt = a;
        m.seasHiFt = std::max(m.seasHiFt, b);
      } else if (unit == "ft" && detail) {
        // "W 12 ft at 14 seconds"
        if (j + 2 < w.size() && w[j + 1] == "at" && isNum(w[j + 2]) && b > bestFt) { bestFt = b; m.periodSec = atoi(w[j + 2].c_str()); }
      }
      i = j - 1;
    }
    if (windy && m.dir.empty() && !dir.empty() && !seas && !detail) m.dir = dir;
  }
  m.ok = m.windHi >= 0 || m.seasHiFt >= 0;
  return m;
}

std::map<std::string, MarineWx> marineByDate(const MarineFc& fc) {
  std::map<std::string, MarineWx> day, night;
  if (!fc.updated) return day;
  LocalTime u = toLocal(fc.updated);
  int64_t base = fromLocal(u.y, u.mo, u.d, 12, 0, 0);
  static const char* WD[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  for (auto& p : fc.periods) {
    bool isNight = p.name.find("Night") != std::string::npos || p.name == "Tonight" || p.name == "Overnight";
    int64_t when = 0;
    if (p.name == "Today" || p.name == "This Afternoon" || p.name == "Rest of Today" || p.name == "Tonight" || p.name == "Overnight") when = base;
    else
      for (int k = 0; k < 7; k++)
        if (p.name.rfind(WD[k], 0) == 0) {
          for (int add = 1; add <= 7; add++) {   // first date after the issue date with that weekday
            int64_t c = base + add * 86400;
            if (toLocal(c).wday == k) { when = c; break; }
          }
          break;
        }
    if (!when) continue;
    std::string date = localDate(when);
    MarineWx m = parseMarineText(p.text);
    if (!m.ok) continue;
    auto& slot = isNight ? night : day;
    if (!slot.count(date)) slot[date] = m;
  }
  for (auto& kv : night) if (!day.count(kv.first)) day[kv.first] = kv.second;
  return day;
}

std::string compassZh(const std::string& d) {
  if (d == "Variable") return "风向不定";
  static const char* M[][2] = {{"N", "北"}, {"NNE", "北"}, {"NE", "东北"}, {"ENE", "东"}, {"E", "东"}, {"ESE", "东"}, {"SE", "东南"}, {"SSE", "南"},
                               {"S", "南"}, {"SSW", "南"}, {"SW", "西南"}, {"WSW", "西"}, {"W", "西"}, {"WNW", "西"}, {"NW", "西北"}, {"NNW", "北"}};
  for (auto& e : M) if (d == e[0]) return e[1];
  return "";
}

std::string compassEn(const std::string& d) {
  if (d == "Variable") return "Var";
  static const char* M[][2] = {{"NNE", "N"}, {"ENE", "E"}, {"ESE", "E"}, {"SSE", "S"}, {"SSW", "S"}, {"WSW", "W"}, {"WNW", "W"}, {"NNW", "N"}};
  for (auto& e : M) if (d == e[0]) return e[1];
  return d;
}

}  // namespace core
