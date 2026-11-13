#include "parse.h"
#include "tz.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>

namespace core {

static int64_t timeOf(JsonVariantConst v) {
  int64_t t = 0;
  if (v.is<const char*>()) parseTime(v.as<const char*>(), t);
  return t;
}
static double num(JsonVariantConst v, double dflt = NAN) { return v.isNull() ? dflt : v.as<double>(); }

bool parseNwsAlerts(const std::string& body, std::vector<RawAlert>& out) {
  out.clear();
  JsonDocument filter;
  JsonObject p = filter["features"][0]["properties"].to<JsonObject>();
  for (const char* k : {"id", "event", "status", "messageType", "areaDesc", "description", "instruction", "onset", "effective", "ends", "expires"}) p[k] = true;
  p["affectedZones"] = true;
  p["parameters"]["VTEC"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter)) != DeserializationError::Ok) return false;
  if (!doc["features"].is<JsonArrayConst>()) return false;
  for (JsonObjectConst f : doc["features"].as<JsonArrayConst>()) {
    JsonObjectConst q = f["properties"];
    RawAlert a;
    a.id = q["id"] | ""; a.event = q["event"] | ""; a.status = q["status"] | ""; a.messageType = q["messageType"] | "";
    a.areaDesc = q["areaDesc"] | ""; a.description = q["description"] | ""; a.instruction = q["instruction"] | "";
    a.onset = timeOf(q["onset"]); a.effective = timeOf(q["effective"]); a.ends = timeOf(q["ends"]); a.expires = timeOf(q["expires"]);
    if (q["parameters"]["VTEC"].is<JsonArrayConst>() && q["parameters"]["VTEC"].size() > 0) a.vtec = q["parameters"]["VTEC"][0] | "";
    for (JsonVariantConst z : q["affectedZones"].as<JsonArrayConst>()) {
      std::string s = z | "";
      size_t k = s.rfind('/');
      a.zones.push_back(k == std::string::npos ? s : s.substr(k + 1));
    }
    out.push_back(a);
  }
  return true;
}

bool parseOmForecast(const std::string& body, OmForecast& f) {
  f = OmForecast{};
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok || !doc["daily"].is<JsonObjectConst>()) return false;
  JsonObjectConst d = doc["daily"];
  for (JsonVariantConst v : d["time"].as<JsonArrayConst>()) f.date.push_back(v | "");
  size_t n = f.date.size();
  auto arr = [&](const char* k, std::vector<double>& o) { o.clear(); JsonArrayConst a = d[k]; for (size_t i = 0; i < n; i++) o.push_back(num(a[i])); };
  arr("temperature_2m_max", f.tmax); arr("temperature_2m_min", f.tmin); arr("wind_speed_10m_max", f.wmax);
  arr("wind_gusts_10m_max", f.gmax); arr("wind_direction_10m_dominant", f.wdir);
  for (size_t i = 0; i < n; i++) {
    f.code.push_back(d["weather_code"][i].isNull() ? -1 : d["weather_code"][i].as<int>());
    f.sunrise.push_back(d["sunrise"][i] | ""); f.sunset.push_back(d["sunset"][i] | "");
  }
  JsonObjectConst c = doc["current"];
  if (!c.isNull()) {
    f.hasCurrent = true;
    f.curTime = timeOf(c["time"]);
    f.curTemp = num(c["temperature_2m"]); f.curWind = num(c["wind_speed_10m"]); f.curGust = num(c["wind_gusts_10m"]);
    f.curDir = num(c["wind_direction_10m"]); f.curCode = c["weather_code"] | -1;
  }
  return n > 0;
}

bool parseOmMarine(const std::string& body, OmMarine& m) {
  m = OmMarine{};
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return false;
  JsonObjectConst d = doc["daily"];
  for (JsonVariantConst v : d["time"].as<JsonArrayConst>()) m.date.push_back(v | "");
  for (size_t i = 0; i < m.date.size(); i++) {
    m.hmax.push_back(num(d["wave_height_max"][i])); m.pmax.push_back(num(d["wave_period_max"][i])); m.dir.push_back(num(d["wave_direction_dominant"][i]));
  }
  JsonObjectConst c = doc["current"];
  if (!c.isNull()) {
    m.curTime = timeOf(c["time"]);
    m.curH = num(c["wave_height"], -1); m.curP = num(c["wave_period"], -1); m.curSst = num(c["sea_surface_temperature"], -1000);
  }
  return !m.date.empty() || !c.isNull();
}

bool parseCoopsHilo(const std::string& body, std::vector<TidePoint>& out) {
  out.clear();
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok || !doc["predictions"].is<JsonArrayConst>()) return false;
  for (JsonObjectConst p : doc["predictions"].as<JsonArrayConst>()) {
    TidePoint t{};
    if (!parseTime(p["t"] | "", t.t)) continue;
    t.h = atof(p["v"] | "0");
    t.high = std::string(p["type"] | "") == "H";
    out.push_back(t);
  }
  return !out.empty();
}

bool parseCoopsWaterTemp(const std::string& body, double& c, int64_t& t) {
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok || !doc["data"].is<JsonArrayConst>() || doc["data"].size() == 0) return false;
  JsonObjectConst last = doc["data"][doc["data"].size() - 1];
  const char* v = last["v"] | "";
  if (!*v) return false;
  c = atof(v);
  return parseTime(last["t"] | "", t);
}

// "Key: 6.6 ft" -> 6.6 ; the degree sign is a Latin-1 0xB0 byte and is ignored.
static bool firstNumber(const std::string& s, double& v) {
  for (size_t i = 0; i < s.size(); i++) {
    if ((s[i] >= '0' && s[i] <= '9') || (s[i] == '-' && i + 1 < s.size() && isdigit((unsigned char)s[i + 1]))) { v = atof(s.c_str() + i); return true; }
  }
  return false;
}

bool parseNdbcLatest(const std::string& body, BuoyObs& o) {
  o = BuoyObs{};
  std::istringstream in(body);
  std::string line;
  bool inSummary = false, gmtSeen = false;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line == "Wave Summary") { inSummary = true; continue; }
    // observation time. Prefer the UTC line "0800 GMT 11/12/26": the local line sometimes has no date ("12:00 am PST").
    int h, mi, mo, d, y; char ap[3] = {0}, zone[4] = {0};
    int hm;
    if (!gmtSeen && sscanf(line.c_str(), "%4d GMT %d/%d/%d", &hm, &mo, &d, &y) == 4) {
      o.t = daysFromCivil(2000 + y, mo, d) * 86400 + (hm / 100) * 3600 + (hm % 100) * 60;
      gmtSeen = true;
      continue;
    }
    if (!o.t && sscanf(line.c_str(), "%d:%d %2s %3s %d/%d/%d", &h, &mi, ap, zone, &mo, &d, &y) == 7) {
      if (h == 12) h = 0;
      if (ap[0] == 'p') h += 12;
      int64_t wall = daysFromCivil(2000 + y, mo, d) * 86400 + h * 3600 + mi * 60;
      o.t = wall + (strcmp(zone, "PDT") == 0 ? 7 : 8) * 3600;
      continue;
    }
    size_t c = line.find(':');
    if (c == std::string::npos) continue;
    std::string key = line.substr(0, c), val = line.substr(c + 1);
    double v;
    if (!firstNumber(val, v)) continue;
    if (key == "Seas") o.seasFt = v;
    else if (key == "Peak Period") o.peakPeriod = v;
    else if (key == "Water Temp") o.waterF = v;
    else if (inSummary && key == "Swell") o.swellFt = v;
    else if (inSummary && key == "Period" && o.swellPeriod < 0) o.swellPeriod = v;
  }
  o.ok = o.t != 0;
  return o.ok;
}

}  // namespace core
