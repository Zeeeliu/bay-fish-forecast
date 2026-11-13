// Data model (PRD §8). In-memory only; the cache is the raw responses (PRD §10.6).
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace core {

// ---------- configuration (PRD §4) ----------
enum class Profile { Coastal, Bay };
enum class Lang { Zh, En };   // landing-page language (detail page is always Chinese)

struct PlaceCfg {
  const char* id;
  const char* name;
  Profile profile;
  double landLat, landLon;          // NWS alert point query
  const char* marineZone;           // PZZxxx
  const char* tideStation;          // CO-OPS id
  const char* tideStationName;      // shown in the chart header
  const char* buoy;                 // NDBC id or "" (bay)
  double fcLat, fcLon;              // Open-Meteo forecast point
  double mrLat, mrLon;              // Open-Meteo marine point
  const char* sourcesText;          // footer right
  const char* nameEn;
  const char* tideStationNameEn;
  const char* sourcesTextEn;
};

const std::vector<PlaceCfg>& places();
extern const char* kBayWaterStation;   // 9414750 Alameda

// ---------- raw inputs (cached responses) ----------
struct Fetched {
  std::string body;          // last good response
  int64_t okAt = 0;          // when body was fetched (UTC), 0 = never
  bool lastOk = false;       // did the latest attempt succeed?
};

struct Inputs {
  std::map<std::string, Fetched> src;   // keys: S1_<place>, S2_<zone>, S3_<place>, S4_<place>, S5_<station>, S6_<buoy>, S7_<station>
  bool clockValid = true;
  int64_t lastFullRefreshAt = 0;        // UTC of the last full refresh attempt
  bool refreshing = false;              // a full refresh is running
  bool wifiConnected = true;
};

// ---------- output ----------
enum class AlertState { None, Issued, Unknown };

struct Alert {
  std::string id, event, nameZh, vtecAction;
  int priority = 40;
  int64_t start = 0, end = 0;
  bool inEffect = false;
  std::string periodText, areaText, shortTpl, fullTpl, descKey;
  std::vector<std::string> facts, chips;
};

struct Banner { std::string title, countText, sub, until, next; };

struct PlaceAlerts {
  AlertState state = AlertState::Unknown;
  int64_t checkedAt = 0;                 // last successful check for this place
  Banner banner;
  std::vector<Alert> items;              // in effect first (by priority), then upcoming (by onset)
  std::vector<std::string> productFacts;
  std::map<std::string, std::string> englishByKey;
};

struct Tile { std::string label, value, unit, sub; };

struct DayCard {
  std::string date, day, md, icon, wx, temp, wind, row2Label, row2, tide;
  bool alert = false;
};

struct TideExtreme { int minute; double h; bool high; };   // minutes from today's local 00:00

struct TideDay {
  bool ok = false;
  std::string station;
  std::vector<TideExtreme> extremes;     // including neighbours outside today, for the curve
  int dayMinutes = 1440;
  double rangeM = 0;
  int sunriseMin = -1, sunsetMin = -1;
  std::string sunrise, sunset;
};

struct PlaceReport {
  std::string id, name;
  Profile profile = Profile::Coastal;
  std::string sourcesText;
  PlaceAlerts alerts;
  TideDay tide;
  Tile wind, wave, weather, water;
  std::vector<DayCard> days;            // always 5
};

struct Report {
  Lang lang = Lang::Zh;
  int64_t now = 0;
  std::string dateText;                 // "11月12日 周四"
  std::string footerLeft;
  bool forecastStale = false;
  std::string status;                   // 已连接 / 正在更新… / 更新失败 · 显示缓存 / 未连接 Wi-Fi
  bool statusError = false;
  std::vector<PlaceReport> places;
};

// Build the report for `now` from cached inputs (PRD §7, §7.7). Pure function.
Report buildReport(const Inputs& in, int64_t now, Lang lang = Lang::Zh);
std::string reportJson(const Report& r);           // serial `report` dump (PRD §8)
double tideAt(const std::vector<TideExtreme>& ex, double minute);   // PRD §7.5

}  // namespace core
