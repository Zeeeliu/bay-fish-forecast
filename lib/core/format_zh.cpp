#include "format_zh.h"
#include "tz.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace core {

static const EventInfo kEvents[] = {
  {"Tsunami Warning", 100, "海啸警报", "立即远离海岸前往高处。", "海啸警报！立即远离海岸前往高处。", "Move away from the coast to high ground now."},
  {"Tsunami Advisory", 95, "海啸提示", "远离海滩和港口。", "可能有强海流和危险海浪，远离海滩和港口。", "Stay away from beaches and harbors."},
  {"Tsunami Watch", 90, "海啸预警", "留意后续通知。", "可能发生海啸，留意后续通知。", "Watch for further updates."},
  {"Storm Warning", 85, "风暴警报", "海上风暴级大风，切勿出海。", "海上风暴级大风（48 节以上），切勿出海。", "Storm-force winds at sea; do not go out."},
  {"Special Marine Warning", 84, "特别海事警报", "船只立即回港避险。", "短时强雷暴或水龙卷，船只立即回港避险。", "Boats return to harbor now."},
  {"High Surf Warning", 82, "巨浪警报", "危险巨浪，切勿下水，远离礁石和海边。", "危险巨浪，海浪可冲上海滩和防波堤。切勿下水，远离礁石和海边。", "Dangerous surf; stay out of the water and off the rocks."},
  {"Hazardous Seas Warning", 80, "危险海况警报", "海况危险，船只避免出海。", "海况危险，浪高且陡，船只避免出海。", "Hazardous seas; boats should stay in port."},
  {"Gale Warning", 78, "大风警报（海上）", "海上大风，小船切勿出海。", "海上大风（34–47 节），小船切勿出海。", "Gale-force winds at sea; small boats stay in."},
  {"Coastal Flood Warning", 76, "沿海洪水警报", "沿岸低洼地区将被淹，避开海边道路。", "沿岸低洼地区将被海水淹没。避开海边道路、停车场和码头。", "Low-lying shore areas will flood; avoid coastal roads."},
  {"High Wind Warning", 74, "强风警报", "强风，避免海边活动。", "强风，避免海边活动。", "Strong winds; avoid the shoreline."},
  {"Beach Hazards Statement", 70, "海滩危险声明", "可能出现疯狗浪和强离岸流，勿背对大海。", "可能出现疯狗浪和强离岸流。勿背对大海，远离礁石、防波堤和码头边缘。", "Sneaker waves, rip currents; never turn your back on the sea."},
  {"High Surf Advisory", 68, "大浪提示", "大浪拍岸，远离礁石和防波堤。", "大浪拍岸，海浪可能突然冲高。远离礁石和防波堤，勿背对大海。", "High surf; stay off rocks and jetties."},
  {"Rip Current Statement", 66, "离岸流声明", "强离岸流，勿下水。", "有强离岸流，可把人迅速卷离岸边。勿下水。", "Strong rip currents; stay out of the water."},
  {"Small Craft Advisory", 64, "小型船只提示", "风浪对小船危险，不建议小船出海。", "风浪对小型船只危险，不建议小船出海。", "Hazardous for small boats."},
  {"Coastal Flood Advisory", 62, "沿海洪水提示", "涨潮时低洼处可能进水。", "涨潮时沿岸低洼地区可能进水。注意高潮时段。", "Minor flooding of low spots at high tide."},
  {"Wind Advisory", 60, "大风提示", "大风，注意安全。", "大风，注意安全。", "Windy; take care."},
  {"Dense Fog Advisory", 58, "浓雾提示", "浓雾，能见度很低。", "浓雾，能见度很低，航行和驾车请小心。", "Dense fog, very low visibility."},
  {"Coastal Flood Statement", 50, "沿海洪水声明", "高潮时沿岸可能小范围积水。", "高潮时沿岸可能有小范围积水。", "Minor shoreline flooding possible at high tide."},
};

const EventInfo* eventInfo(const std::string& event) {
  for (const auto& e : kEvents) if (event == e.event) return &e;
  return nullptr;
}

std::vector<std::string> eventNames() {
  std::vector<std::string> v;
  for (const auto& e : kEvents) v.push_back(e.event);
  return v;
}

std::string zoneZh(const std::string& z) {
  static const char* Z[][2] = {
    {"CAZ006", "旧金山"}, {"CAZ505", "北湾沿海（含雷耶斯角）"}, {"CAZ506", "北湾内陆谷地"}, {"CAZ508", "旧金山湾沿岸"},
    {"CAZ509", "旧金山半岛沿海"}, {"CAZ529", "蒙特雷湾北部"}, {"CAZ530", "蒙特雷湾南部及大苏尔沿海"},
    {"PZZ545", "雷耶斯角至皮金角沿海水域（10海里内）"}, {"PZZ531", "旧金山湾（海湾大桥以南）"}, {"PZZ535", "蒙特雷湾"},
    {"PZZ560", "皮金角至皮诺斯角沿海水域（10海里内）"},
  };
  for (auto& e : Z) if (z == e[0]) return e[1];
  return "";
}

std::string weatherZh(int c) {
  if (c == 0) return "晴";
  if (c == 1 || c == 2) return "晴间多云";
  if (c == 3) return "阴";
  if (c == 45 || c == 48) return "雾";
  if (c >= 51 && c <= 57) return "毛毛雨";
  if (c == 61 || c == 66 || c == 80) return "小雨";
  if (c == 63 || c == 81) return "中雨";
  if (c == 65 || c == 67 || c == 82) return "大雨";
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return "雪";
  if (c == 95 || c == 96 || c == 99) return "雷雨";
  return "多云";
}

std::string weatherEn(int c) {
  if (c == 0) return "Clear";
  if (c == 1 || c == 2) return "Partly";
  if (c == 3) return "Cloudy";
  if (c == 45 || c == 48) return "Fog";
  if (c >= 51 && c <= 57) return "Drizzle";
  if (c == 61 || c == 66 || c == 80) return "Lt rain";
  if (c == 63 || c == 81) return "Rain";
  if (c == 65 || c == 67 || c == 82) return "Heavy";
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return "Snow";
  if (c == 95 || c == 96 || c == 99) return "T-storm";
  return "Cloudy";
}

std::string windDirEn(double deg) {
  static const char* D[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  if (std::isnan(deg)) return "";
  double x = fmod(fmod(deg, 360) + 360, 360);
  return D[(int)((x + 22.5) / 45.0) % 8];
}
std::string weekdayEn(int wday) { static const char* W[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}; return W[((wday % 7) + 7) % 7]; }
std::string weekdayTimeEn(int64_t utc) { return weekdayEn(toLocal(utc).wday) + " " + hhmm(utc); }
std::string dateHeaderEn(int64_t utc) {
  static const char* M[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  LocalTime t = toLocal(utc);
  char b[32]; snprintf(b, sizeof b, "%s, %s %d", weekdayEn(t.wday).c_str(), M[t.mo - 1], t.d);
  return b;
}

std::string weatherIcon(int c) {
  if (c == 0) return "sun";
  if (c == 1 || c == 2) return "partly";
  if (c == 45 || c == 48) return "fog";
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return "rain";
  if (c >= 95) return "storm";
  return "cloud";
}

std::string windDirZh(double deg) {
  static const char* D[] = {"北", "东北", "东", "东南", "南", "西南", "西", "西北"};
  if (std::isnan(deg)) return "";
  double x = fmod(fmod(deg, 360) + 360, 360);
  return D[(int)((x + 22.5) / 45.0) % 8];
}

static std::string withMinus(const char* s) { return s[0] == '-' ? std::string("−") + (s + 1) : std::string(s); }
std::string fmt1(double v) { char b[24]; snprintf(b, sizeof b, "%.1f", v); if (!strcmp(b, "-0.0")) return "0.0"; return withMinus(b); }
std::string fmt2(double v) { char b[24]; snprintf(b, sizeof b, "%.2f", v); if (!strcmp(b, "-0.00")) return "0.00"; return withMinus(b); }
std::string fmtInt(double v) { char b[24]; snprintf(b, sizeof b, "%ld", lround(v)); return withMinus(b); }

std::string dayLabel(int64_t dayUtc, int64_t now) {
  if (localDate(dayUtc) == localDate(now)) return "今天";
  return weekdayZh(toLocal(dayUtc).wday);
}
std::string mdText(int64_t utc) { LocalTime t = toLocal(utc); char b[12]; snprintf(b, sizeof b, "%d/%d", t.mo, t.d); return b; }
std::string weekdayTime(int64_t utc) { return weekdayZh(toLocal(utc).wday) + " " + hhmm(utc); }
std::string dateHeader(int64_t utc) {
  LocalTime t = toLocal(utc);
  char b[32]; snprintf(b, sizeof b, "%d月%d日 ", t.mo, t.d);
  return b + weekdayZh(t.wday);
}

}  // namespace core
