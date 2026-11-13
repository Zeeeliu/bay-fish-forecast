// Parsers for the raw responses (PRD §6). All tolerate missing fields; failure returns false.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace core {

struct RawAlert {
  std::string id, event, status, messageType, vtec, areaDesc, description, instruction;
  int64_t onset = 0, effective = 0, ends = 0, expires = 0;   // 0 = null
  std::vector<std::string> zones;
};
bool parseNwsAlerts(const std::string& body, std::vector<RawAlert>& out);

struct OmForecast {
  std::vector<std::string> date;      // "2026-11-12"
  std::vector<int> code;
  std::vector<double> tmax, tmin, wmax, gmax, wdir;
  std::vector<std::string> sunrise, sunset;   // "2026-11-12T06:46"
  bool hasCurrent = false;
  int64_t curTime = 0;
  double curTemp = 0, curWind = 0, curGust = 0, curDir = 0;
  int curCode = 0;
};
bool parseOmForecast(const std::string& body, OmForecast& f);

struct OmMarine {
  std::vector<std::string> date;
  std::vector<double> hmax, pmax, dir;          // NaN when null
  int64_t curTime = 0;
  double curH = -1, curP = -1, curSst = -1000;  // negative / -1000 = missing
};
bool parseOmMarine(const std::string& body, OmMarine& m);

struct TidePoint { int64_t t; double h; bool high; };
bool parseCoopsHilo(const std::string& body, std::vector<TidePoint>& out);
bool parseCoopsWaterTemp(const std::string& body, double& c, int64_t& t);

struct BuoyObs {
  int64_t t = 0;
  double seasFt = -1, swellFt = -1, peakPeriod = -1, swellPeriod = -1, waterF = -1000;
  bool ok = false;
};
bool parseNdbcLatest(const std::string& body, BuoyObs& o);

}  // namespace core
