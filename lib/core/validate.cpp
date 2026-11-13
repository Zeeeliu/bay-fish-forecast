#include "validate.h"
#include <cmath>
#include "marine.h"
#include "parse.h"

namespace core {

static bool firstFinite(const std::vector<double>& v, size_t n) {
  if (v.size() < n) return false;
  for (size_t i = 0; i < n; i++) if (!std::isfinite(v[i])) return false;
  return true;
}

bool validBody(const std::string& key, const std::string& body) {
  if (body.empty()) return false;
  std::string src = key.substr(0, key.find('_'));
  if (src == "S1" || src == "S2") { std::vector<RawAlert> a; return parseNwsAlerts(body, a); }
  if (src == "S3") { OmForecast f; return parseOmForecast(body, f) && firstFinite(f.tmax, 5) && firstFinite(f.tmin, 5); }
  if (src == "S4") {
    OmMarine m;
    if (!parseOmMarine(body, m)) return false;
    if (m.date.empty()) return m.curSst > -999;   // bay SST fallback: current only
    return firstFinite(m.hmax, 3);
  }
  if (src == "S5") { std::vector<TidePoint> t; return parseCoopsHilo(body, t) && t.size() >= 8; }
  if (src == "S6") { BuoyObs o; return parseNdbcLatest(body, o) && o.ok; }
  if (src == "S7") { double c; int64_t t; return parseCoopsWaterTemp(body, c, t); }
  if (src == "S8") { MarineFc m; return parseNwsMarine(body, m) && m.periods.size() >= 4; }
  return true;
}

}  // namespace core
