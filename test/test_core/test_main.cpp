// M1 acceptance tests (PRD §12) against real fixtures captured 2026-11-12.
#include <unity.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include "alert_text.h"
#include "marine.h"
#include "model.h"
#include "parse.h"
#include "tz.h"

using namespace core;

static std::string load(const std::string& name) {
  std::ifstream f("fixtures/" + name, std::ios::binary);
  std::stringstream ss; ss << f.rdbuf();
  return ss.str();
}
static int64_t T(const char* iso) { int64_t t = 0; parseTime(iso, t); return t; }

static Inputs fixtures(int64_t alertAt, int64_t fcAt) {
  Inputs in;
  auto put = [&](const std::string& k, const std::string& file, int64_t at) { in.src[k] = {load(file), at, true}; };
  put("S1_halfmoon", "nws_alerts_point_hmb_2026-11-12.json", alertAt);
  put("S1_sf", "nws_alerts_point_sf_2026-11-12.json", alertAt);
  put("S1_redwood", "nws_alerts_point_rwc_2026-11-12.json", alertAt);
  put("S2_PZZ545", "nws_alerts_zone_PZZ545_2026-11-12.json", alertAt);
  put("S2_PZZ531", "nws_alerts_zone_PZZ531_2026-11-12.json", alertAt);
  for (auto [id, f] : {std::pair{"halfmoon", "hmb"}, {"sf", "sf"}, {"redwood", "rwc"}}) {
    put(std::string("S3_") + id, std::string("openmeteo_forecast_") + f + "_2026-11-12.json", fcAt);
    put(std::string("S4_") + id, std::string("openmeteo_marine_") + f + "_2026-11-12.json", fcAt);
  }
  for (auto s : {"9414131", "9414290", "9414523"}) put(std::string("S5_") + s, std::string("coops_hilo_") + s + "_20261111-20261117.json", fcAt);
  put("S6_46012", "ndbc_latest_obs_46012_2026-11-12.txt", fcAt);
  put("S6_46026", "ndbc_latest_obs_46026_2026-11-12.txt", fcAt);
  put("S7_9414750", "coops_watertemp_9414750_latest.json", fcAt);
  put("S8_PZZ545", "nws_marine_PZZ545_2026-11-12.json", fcAt);
  put("S8_PZZ531", "nws_marine_PZZ531_2026-11-12.json", fcAt);
  return in;
}
static const PlaceReport& place(const Report& r, const char* id) { for (auto& p : r.places) if (p.id == id) return p; return r.places[0]; }
static bool startsWith(const std::string& s, const std::string& p) { return s.rfind(p, 0) == 0; }

void test_tz() {
  TEST_ASSERT_EQUAL(-8 * 3600, ptOffsetSec(T("2026-11-12T06:40:00-08:00")));
  TEST_ASSERT_EQUAL(-7 * 3600, ptOffsetSec(T("2026-07-01T12:00:00Z")));
  TEST_ASSERT_EQUAL(-7 * 3600, ptOffsetSec(T("2027-03-14T10:00:00Z")));   // DST starts 2027-03-14 02:00 PST
  TEST_ASSERT_EQUAL(-8 * 3600, ptOffsetSec(T("2027-03-14T09:59:00Z")));
  TEST_ASSERT_EQUAL_STRING("06:40", hhmm(T("2026-11-12T06:40:00-08:00")).c_str());
  TEST_ASSERT_EQUAL(4, toLocal(T("2026-11-12T06:40:00-08:00")).wday);   // Thursday
  TEST_ASSERT_EQUAL(T("2026-11-12T00:15:00-08:00"), T("2026-11-12T00:15"));   // no offset = Pacific local
}

void test_sf_banner_thursday() {
  int64_t now = T("2026-11-12T06:40:00-08:00");
  Report r = buildReport(fixtures(T("2026-11-12T06:32:00-08:00"), T("2026-11-12T05:12:00-08:00")), now);
  auto& p = place(r, "sf");
  TEST_ASSERT_TRUE(p.alerts.state == AlertState::Issued);
  TEST_ASSERT_EQUAL_STRING("共5条", p.alerts.banner.countText.c_str());
  TEST_ASSERT_EQUAL_STRING("官方警示生效中 · 海滩危险声明", p.alerts.banner.title.c_str());
  TEST_ASSERT_EQUAL_STRING("即将：巨浪警报 周五 18:00 起", p.alerts.banner.next.c_str());
  TEST_ASSERT_EQUAL_STRING("至 周五 18:00", p.alerts.banner.until.c_str());
  TEST_ASSERT_TRUE(startsWith(p.alerts.banner.sub, "破浪 4.6–6.1米。"));
  bool tags[5] = {true, true, true, true, false};
  for (int i = 0; i < 5; i++) TEST_ASSERT_EQUAL(tags[i], p.days[i].alert);
  // detail order: in effect first, then upcoming by onset
  const char* order[] = {"海滩危险声明", "沿海洪水提示", "小型船只提示", "巨浪警报", "沿海洪水提示"};
  TEST_ASSERT_EQUAL(5, (int)p.alerts.items.size());
  for (int i = 0; i < 5; i++) TEST_ASSERT_EQUAL_STRING(order[i], p.alerts.items[i].nameZh.c_str());
  TEST_ASSERT_EQUAL(2, (int)p.alerts.productFacts.size());
  TEST_ASSERT_EQUAL_STRING("11/12 12:10 高潮约 2.2米（比正常高 0.4米）", p.alerts.productFacts[0].c_str());
}

void test_sf_facts_per_event() {
  int64_t now = T("2026-11-12T06:40:00-08:00");
  Report r = buildReport(fixtures(T("2026-11-12T06:32:00-08:00"), T("2026-11-12T05:12:00-08:00")), now);
  auto& it = place(r, "sf").alerts.items;
  TEST_ASSERT_EQUAL_STRING("破浪 4.6–6.1米（15–20英尺）", it[0].facts.at(0).c_str());
  TEST_ASSERT_EQUAL_STRING("低洼处可能积水深达 0.5米（1.5英尺）", it[1].facts.at(0).c_str());
  TEST_ASSERT_EQUAL_STRING("破浪 7.6–10.7米（25–35英尺）", it[3].facts.at(0).c_str());
  TEST_ASSERT_EQUAL_STRING("偶有 12.2–15.2米（40–50英尺）的大浪组", it[3].facts.at(1).c_str());
  TEST_ASSERT_EQUAL(0, (int)it[4].facts.size());   // 2nd Coastal Flood Advisory: "run up beaches..." has no number
  TEST_ASSERT_EQUAL(7, (int)it[0].chips.size());
  TEST_ASSERT_EQUAL_STRING("旧金山、蒙特雷湾南部及大苏尔沿海", it[0].areaText.c_str());
}

void test_sf_saturday_top_is_hsw() {
  Report r = buildReport(fixtures(T("2026-11-14T05:55:00-08:00"), T("2026-11-14T05:12:00-08:00")), T("2026-11-14T06:00:00-08:00"));
  auto& p = place(r, "sf");
  TEST_ASSERT_EQUAL_STRING("官方警示生效中 · 巨浪警报", p.alerts.banner.title.c_str());
  TEST_ASSERT_EQUAL(2, (int)p.alerts.items.size());   // BHS, SCA and CFA #1 have ended
}

void test_redwood_banner() {
  Report r = buildReport(fixtures(T("2026-11-12T06:32:00-08:00"), T("2026-11-12T05:12:00-08:00")), T("2026-11-12T06:40:00-08:00"));
  auto& p = place(r, "redwood");
  TEST_ASSERT_EQUAL_STRING("共2条", p.alerts.banner.countText.c_str());
  TEST_ASSERT_EQUAL_STRING("官方警示生效中 · 沿海洪水提示", p.alerts.banner.title.c_str());
  TEST_ASSERT_EQUAL_STRING("即将：小型船只提示 周四 21:00 起", p.alerts.banner.next.c_str());
  TEST_ASSERT_EQUAL_STRING("湾内水况", p.wave.label.c_str());
  TEST_ASSERT_EQUAL_STRING("潮差", p.days[0].row2Label.c_str());
  TEST_ASSERT_EQUAL_STRING("阿拉米达站", p.water.sub.c_str());
}

void test_tide_ranges() {
  Report r = buildReport(fixtures(T("2026-11-12T06:32:00-08:00"), T("2026-11-12T05:12:00-08:00")), T("2026-11-12T06:40:00-08:00"));
  TEST_ASSERT_TRUE(place(r, "halfmoon").tide.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.005, 1.768, place(r, "halfmoon").tide.rangeM);
  TEST_ASSERT_FLOAT_WITHIN(0.005, 1.835, place(r, "sf").tide.rangeM);
  TEST_ASSERT_FLOAT_WITHIN(0.005, 2.448, place(r, "redwood").tide.rangeM);
  auto& ex = place(r, "halfmoon").tide.extremes;
  TEST_ASSERT_FLOAT_WITHIN(0.01, 1.06, tideAt(ex, 400));   // prototype: 1.06 m at 06:40
  TEST_ASSERT_EQUAL_STRING("满11:10 干18:50", place(r, "halfmoon").days[0].tide.c_str());
  TEST_ASSERT_EQUAL_STRING("06:46", place(r, "halfmoon").tide.sunrise.c_str());
}

void test_ndbc_variants() {
  BuoyObs a, b, c;
  TEST_ASSERT_TRUE(parseNdbcLatest(load("ndbc_latest_obs_46012.txt"), a));
  TEST_ASSERT_FLOAT_WITHIN(0.01, 6.6, a.seasFt); TEST_ASSERT_FLOAT_WITHIN(0.01, 12, a.peakPeriod); TEST_ASSERT_FLOAT_WITHIN(0.01, 66.0, a.waterF);
  TEST_ASSERT_EQUAL(T("2026-11-11T23:00:00-08:00"), a.t);
  TEST_ASSERT_TRUE(parseNdbcLatest(load("ndbc_latest_obs_46026.txt"), b));   // summary only
  TEST_ASSERT_TRUE(b.seasFt < 0); TEST_ASSERT_FLOAT_WITHIN(0.01, 6.9, b.swellFt); TEST_ASSERT_FLOAT_WITHIN(0.01, 12.9, b.swellPeriod);
  TEST_ASSERT_TRUE(parseNdbcLatest(load("ndbc_latest_obs_46026_2026-11-12.txt"), c));   // Seas + summary, no water temp
  TEST_ASSERT_FLOAT_WITHIN(0.01, 8.2, c.seasFt); TEST_ASSERT_TRUE(c.waterF < -999);
  BuoyObs e;   // just after midnight the local time line has no date: "12:00 am PST"; the GMT line has it
  TEST_ASSERT_TRUE(parseNdbcLatest(load("ndbc_latest_obs_46012_nodate.txt"), e));
  TEST_ASSERT_EQUAL(T("2026-11-12T08:00:00Z"), e.t);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 7.2, e.seasFt);
}

void test_rebuild_after_midnight() {
  Report r = buildReport(fixtures(T("2026-11-12T23:47:00-08:00"), T("2026-11-12T18:00:00-08:00")), T("2026-11-13T01:00:00-08:00"));
  auto& p = place(r, "halfmoon");
  TEST_ASSERT_EQUAL_STRING("今天", p.days[0].day.c_str());
  TEST_ASSERT_EQUAL_STRING("11/13", p.days[0].md.c_str());
  TEST_ASSERT_TRUE(p.days[4].temp != "—");   // the 6th forecast day fills the last card
  TEST_ASSERT_TRUE(p.tide.ok);
  bool bhs = false;
  for (auto& a : p.alerts.items) if (a.event == "Beach Hazards Statement" && a.inEffect) bhs = true;
  TEST_ASSERT_TRUE(bhs);
  TEST_ASSERT_TRUE(p.alerts.state == AlertState::Unknown);   // last check 73 min ago > 45 min
}

void test_failure_is_unknown() {
  Inputs in = fixtures(T("2026-11-12T06:32:00-08:00"), T("2026-11-12T05:12:00-08:00"));
  in.src["S2_PZZ531"].lastOk = false;
  Report r = buildReport(in, T("2026-11-12T06:40:00-08:00"));
  TEST_ASSERT_TRUE(place(r, "redwood").alerts.state == AlertState::Unknown);
  TEST_ASSERT_TRUE(place(r, "sf").alerts.state == AlertState::Issued);
  in.clockValid = false;
  r = buildReport(in, T("2026-11-12T06:40:00-08:00"));
  TEST_ASSERT_TRUE(place(r, "sf").alerts.state == AlertState::Unknown);
}

void test_none_state() {
  Inputs in = fixtures(T("2026-11-12T06:32:00-08:00"), T("2026-11-12T05:12:00-08:00"));
  std::string empty = R"({"type":"FeatureCollection","features":[]})";
  in.src["S1_redwood"].body = empty; in.src["S2_PZZ531"].body = empty;
  Report r = buildReport(in, T("2026-11-12T06:40:00-08:00"));
  TEST_ASSERT_TRUE(place(r, "redwood").alerts.state == AlertState::None);
  for (auto& d : place(r, "redwood").days) TEST_ASSERT_FALSE(d.alert);
}

void test_report_json() {
  Report r = buildReport(fixtures(T("2026-11-12T06:32:00-08:00"), T("2026-11-12T05:12:00-08:00")), T("2026-11-12T06:40:00-08:00"));
  std::string j = reportJson(r);
  TEST_ASSERT_TRUE(j.size() > 1000 && j.size() < 64 * 1024);
  TEST_ASSERT_TRUE(j.find("\"schema\":3") != std::string::npos);
}

void test_marine_text() {
  auto m = parseMarineText("S wind 25 to 30 kt with gusts up to 40 kt, veering to W 20 to 25 kt in the afternoon. Seas 9 to 12 ft, building to 10 to 14 ft in the afternoon. Wave Detail: W 12 ft at 14 seconds and S 5 ft at 15 seconds. Rain.");
  TEST_ASSERT_EQUAL_STRING("S", m.dir.c_str()); TEST_ASSERT_EQUAL(25, m.windLo); TEST_ASSERT_EQUAL(30, m.windHi); TEST_ASSERT_EQUAL(40, m.gust);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 14, m.seasHiFt); TEST_ASSERT_EQUAL(14, m.periodSec);
  m = parseMarineText("S wind 20 to 25 kt, veering to SW in the afternoon. Gusts up to 40 kt. A chance of rain.");
  TEST_ASSERT_EQUAL_STRING("S", m.dir.c_str()); TEST_ASSERT_EQUAL(25, m.windHi); TEST_ASSERT_EQUAL(40, m.gust); TEST_ASSERT_TRUE(m.seasHiFt < 0);
  m = parseMarineText("SE wind 10 to 15 kt, becoming S 15 to 20 kt in the afternoon. Seas 7 to 10 ft. Wave Detail: W 10 ft at 15 seconds.");
  TEST_ASSERT_EQUAL_STRING("S", m.dir.c_str()); TEST_ASSERT_EQUAL(15, m.windLo); TEST_ASSERT_EQUAL(20, m.windHi); TEST_ASSERT_EQUAL(-1, m.gust);
  m = parseMarineText("W wind around 5 kt, backing to S around 5 kt late. Seas 6 to 9 ft.");
  TEST_ASSERT_EQUAL(5, m.windHi); TEST_ASSERT_EQUAL_STRING("W", m.dir.c_str());
  m = parseMarineText("NW wind 10 to 15 kt. Seas 10 to 14 ft. IN THE DEEP WATER CHANNEL...Mixed seas of 6 to 8 ft. Maximum ebb current of 3.1 kt at 05:26 PM.");
  TEST_ASSERT_EQUAL(15, m.windHi); TEST_ASSERT_FLOAT_WITHIN(0.01, 14, m.seasHiFt);
}

void test_marine_in_report() {
  Report r = buildReport(fixtures(T("2026-11-12T06:32:00-08:00"), T("2026-11-12T05:12:00-08:00")), T("2026-11-12T06:40:00-08:00"));
  auto& h = place(r, "halfmoon");
  TEST_ASSERT_EQUAL_STRING("南 15–20节", h.days[0].wind.c_str());          // Thu: SE 10-15 becoming S 15-20 kt
  TEST_ASSERT_EQUAL_STRING("3.0米 · 15秒", h.days[0].row2.c_str());        // Seas 7 to 10 ft
  TEST_ASSERT_EQUAL_STRING("南 25–30节 阵40", h.days[1].wind.c_str());      // Fri: the case the audit caught
  TEST_ASSERT_EQUAL_STRING("4.3米 · 14秒", h.days[1].row2.c_str());
  TEST_ASSERT_EQUAL_STRING("6.4米 · 18秒", h.days[2].row2.c_str());        // Sat: Seas 16 to 21 ft
  TEST_ASSERT_EQUAL_STRING("今日海上 南 15–20节", h.wind.sub.c_str());
  auto& w = place(r, "redwood");
  TEST_ASSERT_EQUAL_STRING("南 20–25节 阵40", w.days[1].wind.c_str());      // PZZ531 Fri
  TEST_ASSERT_EQUAL_STRING("潮差", w.days[1].row2Label.c_str());
}

void test_fourth_place_without_data() {   // Santa Cruz added; a place with no cached data must be UNKNOWN, never NONE
  Report r = buildReport(fixtures(T("2026-11-12T06:32:00-08:00"), T("2026-11-12T05:12:00-08:00")), T("2026-11-12T06:40:00-08:00"));
  TEST_ASSERT_EQUAL(4, (int)r.places.size());
  auto& p = place(r, "santacruz");
  TEST_ASSERT_EQUAL_STRING("圣克鲁斯", p.name.c_str());
  TEST_ASSERT_TRUE(p.alerts.state == AlertState::Unknown);
  TEST_ASSERT_EQUAL(5, (int)p.days.size());
  TEST_ASSERT_FALSE(p.tide.ok);
}

void test_english_landing() {   // PRD D11: English option for the landing page
  Report r = buildReport(fixtures(T("2026-11-12T06:32:00-08:00"), T("2026-11-12T05:12:00-08:00")), T("2026-11-12T06:40:00-08:00"), Lang::En);
  auto& h = place(r, "halfmoon");
  TEST_ASSERT_EQUAL_STRING("Half Moon Bay", h.name.c_str());
  TEST_ASSERT_EQUAL_STRING("Official alert in effect · Beach Hazards Statement", h.alerts.banner.title.c_str());
  TEST_ASSERT_EQUAL_STRING("4 alerts", h.alerts.banner.countText.c_str());
  TEST_ASSERT_EQUAL_STRING("Breakers 15–20 ft. Sneaker waves, rip currents; never turn your back on the sea.", h.alerts.banner.sub.c_str());
  TEST_ASSERT_EQUAL_STRING("Until Fri 18:00", h.alerts.banner.until.c_str());
  TEST_ASSERT_EQUAL_STRING("Next: High Surf Warning Fri 18:00", h.alerts.banner.next.c_str());
  TEST_ASSERT_EQUAL_STRING("S 25–30kt G40", h.days[1].wind.c_str());
  TEST_ASSERT_EQUAL_STRING("4.3m · 14s", h.days[1].row2.c_str());
  TEST_ASSERT_EQUAL_STRING("H11:10 L18:50", h.days[0].tide.c_str());
  TEST_ASSERT_EQUAL_STRING("Today", h.days[0].day.c_str());
  TEST_ASSERT_EQUAL_STRING("Fri", h.days[1].day.c_str());
  TEST_ASSERT_EQUAL_STRING("Thu, Nov 12", r.dateText.c_str());
  TEST_ASSERT_EQUAL_STRING("Range", place(r, "redwood").days[0].row2Label.c_str());
  // the Chinese report is unchanged and its alert items stay Chinese for the detail page
  Report z = buildReport(fixtures(T("2026-11-12T06:32:00-08:00"), T("2026-11-12T05:12:00-08:00")), T("2026-11-12T06:40:00-08:00"));
  TEST_ASSERT_EQUAL_STRING("官方警示生效中 · 海滩危险声明", place(z, "halfmoon").alerts.banner.title.c_str());
  TEST_ASSERT_EQUAL_STRING("海滩危险声明", place(r, "halfmoon").alerts.items[0].nameZh.c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_tz);
  RUN_TEST(test_sf_banner_thursday);
  RUN_TEST(test_sf_facts_per_event);
  RUN_TEST(test_sf_saturday_top_is_hsw);
  RUN_TEST(test_redwood_banner);
  RUN_TEST(test_tide_ranges);
  RUN_TEST(test_ndbc_variants);
  RUN_TEST(test_rebuild_after_midnight);
  RUN_TEST(test_failure_is_unknown);
  RUN_TEST(test_none_state);
  RUN_TEST(test_report_json);
  RUN_TEST(test_marine_text);
  RUN_TEST(test_marine_in_report);
  RUN_TEST(test_fourth_place_without_data);
  RUN_TEST(test_english_landing);
  return UNITY_END();
}
