// buildReport: PRD §7 (alerts §7.1, text §7.2, formatting §7.4, tide §7.5, staleness §7.6, rebuild §7.7).
#include <algorithm>
#include <cmath>
#include <regex>
#include <set>
#include "alert_text.h"
#include "format_zh.h"
#include "model.h"
#include "marine.h"
#include "parse.h"
#include "tz.h"

namespace core {

static const int64_t kAlertStale = 45 * 60;      // PRD §7.6
static const int64_t kForecastStale = 14 * 3600;
static const int64_t kBuoyFresh = 3 * 3600;

double tideAt(const std::vector<TideExtreme>& ex, double m) {
  for (size_t i = 0; i + 1 < ex.size(); i++) {
    const auto &a = ex[i], &b = ex[i + 1];
    if (m >= a.minute && m <= b.minute) {
      double f = (m - a.minute) / (double)(b.minute - a.minute);
      return a.h + (b.h - a.h) * (1 - cos(M_PI * f)) / 2;
    }
  }
  return ex.empty() ? 0 : (m < ex.front().minute ? ex.front().h : ex.back().h);
}

static const Fetched* get(const Inputs& in, const std::string& k) {
  auto it = in.src.find(k);
  return it == in.src.end() || it->second.okAt == 0 ? nullptr : &it->second;
}
static int64_t dayStartOf(const std::string& date) {   // "2026-11-12" -> UTC of local midnight
  int y, m, d;
  if (sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return 0;
  return fromLocal(y, m, d, 0, 0, 0);
}
static int64_t nextDayStart(int64_t dayStart) {
  LocalTime t = toLocal(dayStart + 12 * 3600);
  return fromLocal(t.y, t.mo, t.d + 1, 0, 0, 0);
}
static std::string stripParen(const std::string& s) {   // "破浪 4.6–6.1米（15–20英尺）" -> "破浪 4.6–6.1米"; "Breaking waves 15–20 ft (…)" -> "…ft"
  size_t p = s.find("（");
  if (p == std::string::npos) p = s.find(" (");
  return p == std::string::npos ? s : s.substr(0, p);
}

// ---------------- alerts (§7.1, §7.2) ----------------
static PlaceAlerts buildAlerts(const Inputs& in, const PlaceCfg& pc, int64_t now, bool en) {
  PlaceAlerts pa;
  std::string k1 = std::string("S1_") + pc.id, k2 = std::string("S2_") + pc.marineZone;
  const Fetched *f1 = get(in, k1), *f2 = get(in, k2);
  auto attemptOk = [&](const std::string& k) { auto it = in.src.find(k); return it != in.src.end() && it->second.lastOk; };

  std::vector<RawAlert> raw;
  std::set<std::string> seen;
  for (const Fetched* f : {f1, f2}) {
    if (!f) continue;
    std::vector<RawAlert> v;
    if (!parseNwsAlerts(f->body, v)) continue;
    for (auto& a : v) if (seen.insert(a.id).second) raw.push_back(a);
  }
  pa.checkedAt = (f1 && f2) ? std::min(f1->okAt, f2->okAt) : 0;
  bool unknown = !in.clockValid || !f1 || !f2 || !attemptOk(k1) || !attemptOk(k2) || now - pa.checkedAt > kAlertStale;

  // filter (§7.1.1)
  std::vector<RawAlert> keep;
  for (auto& a : raw) {
    bool cancelled = a.vtec.find(".CAN.") != std::string::npos || a.vtec.find(".EXP.") != std::string::npos;
    int64_t end = a.ends ? a.ends : a.expires;
    if (a.status != "Actual" || a.messageType == "Cancel" || cancelled) continue;
    if (end && end <= now) continue;
    keep.push_back(a);
  }
  // ordinal rank among features with the same event and description, by onset (§7.2 layer 2)
  auto startOf = [](const RawAlert& a) { return a.onset ? a.onset : a.effective; };
  std::set<std::string> factSeen;
  std::map<std::string, std::vector<std::string>> clausesById;
  for (auto& a : keep) {
    Alert x;
    x.id = a.id; x.event = a.event;
    x.start = startOf(a); x.end = a.ends ? a.ends : a.expires;
    x.inEffect = x.start <= now && now < x.end;
    std::smatch m; static const std::regex vt(R"(/[A-Z]\.([A-Z]{3})\.)");
    if (std::regex_search(a.vtec, m, vt)) x.vtecAction = m[1];
    const EventInfo* ei = eventInfo(a.event);
    x.priority = ei ? ei->priority : 40;
    x.nameZh = ei ? ei->nameZh : a.event;
    x.shortTpl = ei ? ei->shortTpl : "请查看英文原文。";
    x.fullTpl = ei ? ei->fullTpl : "请查看英文原文。";
    x.periodText = weekdayTime(x.start) + " 至 " + weekdayTime(x.end);
    bool allKnown = !a.zones.empty();
    std::string area;
    for (auto& z : a.zones) { std::string c = zoneZh(z); if (c.empty()) allKnown = false; else area += (area.empty() ? "" : "、") + c; }
    x.areaText = allKnown ? area : a.areaDesc;

    auto secs = splitSections(a.description);
    std::string what = sectionText(secs, "WHAT");
    auto clauses = eventClauses(what);
    int rank = 0;
    for (auto& b : keep) if (&b != &a && b.event == a.event && b.description == a.description && startOf(b) < x.start) rank++;
    std::vector<std::string> mine;
    if (clauses.empty()) mine.push_back(what);
    else {
      std::vector<Clause> forMe;
      for (auto& c : clauses) if (c.event == a.event) forMe.push_back(c);
      for (auto& c : forMe) if (forMe.size() == 1 || c.ordinal == rank) mine.push_back(c.text);
    }
    for (auto& t : mine) for (auto& f : extractFacts(t)) x.facts.push_back(f);
    clausesById[x.id] = mine;
    x.chips = keywordChips(a.description + " " + a.instruction);
    x.descKey = hashKey(a.description);
    if (!pa.englishByKey.count(x.descKey)) {
      pa.englishByKey[x.descKey] = englishText(a.description, a.instruction);
      for (auto& f : extractFacts(sectionText(secs, "ADDITIONAL DETAILS"))) if (factSeen.insert(f).second) pa.productFacts.push_back(f);
    }
    pa.items.push_back(x);
  }
  std::stable_sort(pa.items.begin(), pa.items.end(), [](const Alert& a, const Alert& b) {
    if (a.inEffect != b.inEffect) return a.inEffect;
    if (a.inEffect) return a.priority > b.priority;
    return a.start < b.start;
  });

  if (unknown) { pa.state = AlertState::Unknown; return pa; }
  if (pa.items.empty()) { pa.state = AlertState::None; return pa; }
  pa.state = AlertState::Issued;
  // banner (§7.1.4): items are sorted, so items[0] is the top in-effect alert or the earliest upcoming one
  const Alert& top = pa.items[0];
  const std::vector<std::string>& topWhatClauses = clausesById[top.id];
  const Alert* next = nullptr;
  for (auto& a : pa.items)
    if (&a != &top && !a.inEffect && (!next || a.priority > next->priority || (a.priority == next->priority && a.start < next->start))) next = &a;
  Banner& b = pa.banner;
  if (en) {
    const EventInfo* ei = eventInfo(top.event);
    std::string tpl = ei ? ei->shortEn : "See the full NWS text.";
    std::vector<std::string> f;
    for (auto& t : topWhatClauses) for (auto& x : extractFacts(t, true)) f.push_back(x);
    b.title = top.inEffect ? "Official alert in effect · " + top.event : "Official alert issued · " + top.event + " · from " + weekdayTimeEn(top.start);
    b.countText = pa.items.size() >= 2 ? std::to_string(pa.items.size()) + " alerts" : "";
    b.sub = f.empty() ? tpl : stripParen(f[0]) + ". " + tpl;
    b.until = "Until " + weekdayTimeEn(top.end);
    if (next) b.next = "Next: " + next->event + " " + weekdayTimeEn(next->start);
    return pa;
  }
  b.title = top.inEffect ? "官方警示生效中 · " + top.nameZh : "官方警示已发布 · " + top.nameZh + " · " + weekdayTime(top.start) + " 起生效";
  b.countText = pa.items.size() >= 2 ? "共" + std::to_string(pa.items.size()) + "条" : "";
  b.sub = top.facts.empty() ? top.shortTpl : stripParen(top.facts[0]) + "。" + top.shortTpl;
  b.until = "至 " + weekdayTime(top.end);
  if (next) b.next = "即将：" + next->nameZh + " " + weekdayTime(next->start) + " 起";
  return pa;
}

// ---------------- tide (§7.5) ----------------
static void tideForDay(const std::vector<TidePoint>& pts, int64_t dayStart, int64_t dayEnd, double& hi, double& lo,
                       const TidePoint*& hiP, const TidePoint*& loP) {
  hiP = loP = nullptr;
  for (auto& p : pts) {
    if (p.t < dayStart || p.t >= dayEnd) continue;
    if (p.high && (!hiP || p.h > hiP->h)) hiP = &p;
    if (!p.high && (!loP || p.h < loP->h)) loP = &p;
  }
  hi = hiP ? hiP->h : NAN; lo = loP ? loP->h : NAN;
}

// ---------------- report ----------------
Report buildReport(const Inputs& in, int64_t now, Lang lang) {
  const bool en = lang == Lang::En;
  auto L = [en](const std::string& zh, const std::string& e) { return en ? e : zh; };
  Report r;
  r.lang = lang;
  r.now = now;
  r.dateText = en ? dateHeaderEn(now) : dateHeader(now);
  int64_t today = localMidnight(now), tomorrow = nextDayStart(today);
  std::string todayStr = localDate(now);

  int64_t fcAt = 0, alertAt = INT64_MAX;
  for (auto& pc : places()) {
    if (auto f = get(in, std::string("S3_") + pc.id)) fcAt = std::max(fcAt, f->okAt);
    for (std::string k : {std::string("S1_") + pc.id, std::string("S2_") + pc.marineZone}) {
      auto f = get(in, k);
      alertAt = std::min(alertAt, f ? f->okAt : 0);
    }
  }

  for (auto& pc : places()) {
    PlaceReport p;
    p.id = pc.id; p.name = L(pc.name, pc.nameEn); p.profile = pc.profile; p.sourcesText = L(pc.sourcesText, pc.sourcesTextEn);
    p.alerts = buildAlerts(in, pc, now, en);

    OmForecast fc; bool haveFc = false;
    if (auto f = get(in, std::string("S3_") + pc.id)) haveFc = parseOmForecast(f->body, fc);
    OmMarine mr; bool haveMr = false;
    if (auto f = get(in, std::string("S4_") + pc.id)) haveMr = parseOmMarine(f->body, mr);
    // official NWS marine zone forecast: wind, gusts, seas (PRD D5 rev.); ignored when older than the forecast staleness limit
    std::map<std::string, MarineWx> marine;
    if (auto f = get(in, std::string("S8_") + pc.marineZone)) {
      MarineFc mf;
      if (now - f->okAt < kForecastStale && parseNwsMarine(f->body, mf)) marine = marineByDate(mf);
    }
    auto marineWind = [en](const MarineWx& m) {   // "南 25–30节 阵40" / "S 25–30kt G40"
      std::string r = en ? compassEn(m.dir) : compassZh(m.dir);
      r += (r.empty() ? "" : " ") + (m.windLo >= 0 && m.windLo < m.windHi ? std::to_string(m.windLo) + "–" : std::string("")) + std::to_string(m.windHi) + (en ? "kt" : "节");
      if (m.gust > 0) r += (en ? " G" : " 阵") + std::to_string(m.gust);
      return r;
    };
    auto dirOf = [en](double deg) { return en ? windDirEn(deg) : windDirZh(deg); };
    auto wxOf = [en](int code) { return en ? weatherEn(code) : weatherZh(code); };
    std::vector<TidePoint> tides;
    if (auto f = get(in, std::string("S5_") + pc.tideStation)) parseCoopsHilo(f->body, tides);
    int ti = -1;
    for (size_t i = 0; haveFc && i < fc.date.size(); i++) if (fc.date[i] == todayStr) ti = (int)i;

    // tide day
    TideDay& td = p.tide;
    td.station = L(pc.tideStationName, pc.tideStationNameEn);
    td.dayMinutes = (int)((tomorrow - today) / 60);
    for (auto& t : tides) {
      int m = (int)((t.t - today) / 60);
      if (m >= -1440 && m <= td.dayMinutes + 1440) td.extremes.push_back({m, t.h, t.high});
    }
    bool before = false, after = false;
    double mx = -1e9, mn = 1e9;
    for (auto& e : td.extremes) {
      if (e.minute <= 0) before = true;
      if (e.minute >= td.dayMinutes) after = true;
      if (e.minute >= 0 && e.minute < td.dayMinutes) { mx = std::max(mx, e.h); mn = std::min(mn, e.h); }
    }
    td.ok = before && after && mx > mn;
    td.rangeM = td.ok ? mx - mn : 0;
    if (ti >= 0) {
      int64_t sr, ss;
      if (parseTime(fc.sunrise[ti], sr)) { td.sunrise = hhmm(sr); td.sunriseMin = (int)((sr - today) / 60); }
      if (parseTime(fc.sunset[ti], ss)) { td.sunset = hhmm(ss); td.sunsetMin = (int)((ss - today) / 60); }
    }

    // tiles (§7.4)
    const std::string dash = "—", noData = L("暂无数据", "No data");
    if (haveFc && fc.hasCurrent && ti >= 0) {
      p.wind = {L("风 · " + hhmm(fc.curTime) + " 实况", "Wind · " + hhmm(fc.curTime) + " obs"), dirOf(fc.curDir) + " " + fmtInt(fc.curWind), L("节", "kt"),
                L("阵风 " + fmtInt(fc.curGust) + " · 今日最大 " + fmtInt(fc.gmax[ti]), "Gusts " + fmtInt(fc.curGust) + " · max " + fmtInt(fc.gmax[ti]))};
      auto mt = marine.find(todayStr);
      if (mt != marine.end() && mt->second.windHi >= 0)
        p.wind.sub = en ? "Sea today: " + (mt->second.gust > 0 ? "gusts " + std::to_string(mt->second.gust) + "kt" : marineWind(mt->second))
                        : "今日海上 " + (mt->second.gust > 0 ? "阵风 " + std::to_string(mt->second.gust) + "节" : marineWind(mt->second));
      p.weather = {L("天气 · 气温", "Weather · temp"), fmtInt(fc.tmin[ti]) + "–" + fmtInt(fc.tmax[ti]) + "°C", "",
                   wxOf(fc.code[ti]) + L(" · 实况 ", " · now ") + fmtInt(fc.curTemp) + "°C"};
    } else {
      p.wind = {L("风", "Wind"), dash, "", noData};
      p.weather = {L("天气 · 气温", "Weather · temp"), dash, "", noData};
    }
    BuoyObs ob; bool haveOb = false;
    if (*pc.buoy) if (auto f = get(in, std::string("S6_") + pc.buoy)) haveOb = parseNdbcLatest(f->body, ob) && now - ob.t < kBuoyFresh;
    if (pc.profile == Profile::Coastal) {
      double ft = haveOb ? (ob.seasFt >= 0 ? ob.seasFt : ob.swellFt) : -1;
      if (ft >= 0) {
        double per = ob.peakPeriod >= 0 ? ob.peakPeriod : ob.swellPeriod;
        p.wave = {L("外海浪高 · 浮标", "Offshore · buoy"), fmt1(ft * 0.3048), L("米", "m"),
                  L("约" + fmt1(ft) + "英尺" + (per >= 0 ? " · 周期" + fmtInt(per) + "秒" : ""), "~" + fmt1(ft) + " ft" + (per >= 0 ? " · " + fmtInt(per) + " s period" : ""))};
      } else if (haveMr && mr.curH >= 0) {
        p.wave = {L("外海浪高 · 预报", "Offshore · fcst"), fmt1(mr.curH), L("米", "m"),
                  L("约" + fmt1(mr.curH / 0.3048) + "英尺" + (mr.curP >= 0 ? " · 周期" + fmtInt(mr.curP) + "秒" : ""), "~" + fmt1(mr.curH / 0.3048) + " ft" + (mr.curP >= 0 ? " · " + fmtInt(mr.curP) + " s period" : ""))};
      } else p.wave = {L("外海浪高", "Offshore waves"), dash, "", noData};
      if (haveOb && ob.waterF > -1000) p.water = {L("水温", "Water temp"), fmtInt((ob.waterF - 32) * 5 / 9) + "°C", "", L("浮标 ", "Buoy ") + pc.buoy};
      else if (haveMr && mr.curSst > -1000) p.water = {L("水温", "Water temp"), fmtInt(mr.curSst) + "°C", "", L("海温模型估计", "Model estimate")};
      else p.water = {L("水温", "Water temp"), dash, "", noData};
    } else {
      if (ti >= 0) {
        long w = lround(fc.wmax[ti]);
        p.wave = en ? Tile{"Bay chop", w < 8 ? "Calm" : w <= 14 ? "Light" : w <= 20 ? "Moderate" : "Rough", "", "Estimated from wind"}
                    : Tile{"湾内水况", w < 8 ? "平静" : w <= 14 ? "小浪" : w <= 20 ? "中浪" : "浪大", "", "按风力估计"};
      } else p.wave = {L("湾内水况", "Bay chop"), dash, "", noData};
      double c; int64_t t;
      auto f7 = get(in, std::string("S7_") + kBayWaterStation);
      if (f7 && now - f7->okAt < kForecastStale && parseCoopsWaterTemp(f7->body, c, t)) p.water = {L("水温", "Water temp"), fmtInt(c) + "°C", "", L("阿拉米达站", "Alameda station")};
      else if (haveMr && mr.curSst > -1000) p.water = {L("水温", "Water temp"), fmtInt(mr.curSst) + "°C", "", L("海温模型估计", "Model estimate")};
      else p.water = {L("水温", "Water temp"), dash, "", noData};
    }

    // 5-day cards (§7.4, §7.1.5)
    for (int i = 0; i < 5; i++) {
      DayCard d;
      int idx = ti >= 0 ? ti + i : -1;
      int64_t ds = today;
      for (int k = 0; k < i; k++) ds = nextDayStart(ds);
      int64_t de = nextDayStart(ds);
      d.date = localDate(ds + 3600);
      d.day = i == 0 ? L("今天", "Today") : (en ? weekdayEn(toLocal(ds + 3600).wday) : weekdayZh(toLocal(ds + 3600).wday));
      d.md = mdText(ds + 3600);
      d.row2Label = pc.profile == Profile::Coastal ? L("浪", "Waves") : L("潮差", "Range");
      if (idx >= 0 && idx < (int)fc.date.size()) {
        d.icon = weatherIcon(fc.code[idx]); d.wx = wxOf(fc.code[idx]);
        d.temp = fmtInt(fc.tmin[idx]) + "–" + fmtInt(fc.tmax[idx]) + "°C";
        long w = lround(fc.wmax[idx]), g = lround(fc.gmax[idx]);
        d.wind = dirOf(fc.wdir[idx]) + " " + std::to_string(w) + L("节", "kt") + (g > w ? L(" 阵", " G") + std::to_string(g) : "");   // fallback (model)
      } else { d.icon = "cloud"; d.wx = dash; d.temp = dash; d.wind = dash; }
      auto md = marine.find(d.date);
      bool haveMarine = md != marine.end();
      if (haveMarine && md->second.windHi >= 0) d.wind = marineWind(md->second);   // official NWS marine forecast
      double hi, lo; const TidePoint *hp, *lp;
      tideForDay(tides, ds, de, hi, lo, hp, lp);
      d.tide = (hp ? L("满", "H") + hhmm(hp->t) : std::string("")) + (hp && lp ? " " : "") + (lp ? L("干", "L") + hhmm(lp->t) : std::string(""));
      if (d.tide.empty()) d.tide = dash;
      if (pc.profile == Profile::Coastal) {
        d.row2 = dash;
        if (haveMarine && md->second.seasHiFt >= 0)
          d.row2 = fmt1(md->second.seasHiFt * 0.3048) + L("米", "m") + (md->second.periodSec > 0 ? " · " + std::to_string(md->second.periodSec) + L("秒", "s") : "");
        else for (size_t j = 0; haveMr && j < mr.date.size(); j++)
          if (mr.date[j] == d.date && !std::isnan(mr.hmax[j])) d.row2 = fmt1(mr.hmax[j]) + L("米", "m") + (std::isnan(mr.pmax[j]) ? "" : " · " + fmtInt(mr.pmax[j]) + L("秒", "s"));
      } else d.row2 = (hp && lp) ? fmt1(hi - lo) + L("米", "m") : dash;
      for (auto& a : p.alerts.items) if (a.start < de && a.end > ds) d.alert = true;
      if (p.alerts.state != AlertState::Issued) d.alert = false;
      p.days.push_back(d);
    }
    r.places.push_back(p);
  }

  // footer, staleness, status (§7.6, §9.3)
  r.forecastStale = fcAt == 0 || now - fcAt > kForecastStale;
  int64_t nextFull = 0;
  for (int add = 0; add < 2 && !nextFull; add++)
    for (int h : {5, 12, 18}) {
      LocalTime t = toLocal(now + add * 86400);
      int64_t c = fromLocal(t.y, t.mo, t.d, h, 0, 0);
      if (c > now) { nextFull = c; break; }
    }
  std::string alertTxt = alertAt && alertAt != INT64_MAX ? hhmm(alertAt) : "—";
  if (r.forecastStale) r.footerLeft = fcAt ? L("预报数据已过期（上次 " + hhmm(fcAt) + "）", "Forecast out of date (last " + hhmm(fcAt) + ")") : L("暂无预报数据", "No forecast data");
  else r.footerLeft = L("预报更新 " + hhmm(fcAt) + " · 警示检查 " + alertTxt + "（每15分钟）· 下次完整更新 " + hhmm(nextFull),
                        "Forecast " + hhmm(fcAt) + " · Alerts " + alertTxt + " (every 15 min) · Next " + hhmm(nextFull));
  bool failed = false;
  for (auto& kv : in.src) if (kv.second.okAt == 0 || !kv.second.lastOk) failed = true;
  if (!in.wifiConnected) { r.status = L("未连接 Wi-Fi", "No Wi-Fi"); r.statusError = true; }
  else if (in.refreshing) r.status = L("正在更新…", "Updating…");
  else if (failed) { r.status = L("更新失败 · 显示缓存", "Update failed · cached"); r.statusError = true; }
  else r.status = L("已连接", "Connected");
  return r;
}

}  // namespace core
