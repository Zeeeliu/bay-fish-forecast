// NWS marine zone forecast (PZZ545 coast, PZZ531 bay): official wind / gust / seas per period (PRD D5 rev. 2026-11-12).
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace core {

struct MarinePeriod { std::string name, text; };
struct MarineFc { int64_t updated = 0; std::vector<MarinePeriod> periods; };
bool parseNwsMarine(const std::string& body, MarineFc& out);

struct MarineWx {
  bool ok = false;
  std::string dir;              // compass of the strongest wind ("S", "SW", "Variable")
  int windLo = -1, windHi = -1; // sustained kt (range of the strongest phrase)
  int gust = -1;                // kt, -1 if none stated
  double seasLoFt = -1, seasHiFt = -1;
  int periodSec = -1;           // period of the largest "Wave Detail" component
};
MarineWx parseMarineText(const std::string& text);

// Daytime period for each local date (night period only when no daytime period remains, e.g. "Tonight").
std::map<std::string, MarineWx> marineByDate(const MarineFc& fc);
std::string compassZh(const std::string& dir);
std::string compassEn(const std::string& dir);   // 16-point -> 8-point, "Var"

}  // namespace core
