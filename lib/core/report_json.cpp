// Serial `report` dump (PRD §8).
#include <ArduinoJson.h>
#include "model.h"
#include "tz.h"

namespace core {

static const char* stateName(AlertState s) { return s == AlertState::Issued ? "ISSUED" : s == AlertState::None ? "NONE" : "UNKNOWN"; }
static void tile(JsonObject o, const Tile& t) { o["label"] = t.label; o["value"] = t.value; o["unit"] = t.unit; o["sub"] = t.sub; }

std::string reportJson(const Report& r) {
  JsonDocument d;
  d["schema"] = 3;
  d["now"] = localDate(r.now) + " " + hhmm(r.now);
  d["dateText"] = r.dateText; d["footerLeft"] = r.footerLeft; d["forecastStale"] = r.forecastStale; d["status"] = r.status;
  JsonArray ps = d["places"].to<JsonArray>();
  for (auto& p : r.places) {
    JsonObject o = ps.add<JsonObject>();
    o["id"] = p.id; o["name"] = p.name; o["sourcesText"] = p.sourcesText;
    JsonObject a = o["alerts"].to<JsonObject>();
    a["state"] = stateName(p.alerts.state);
    a["checkedAt"] = p.alerts.checkedAt ? hhmm(p.alerts.checkedAt) : "";
    JsonObject b = a["banner"].to<JsonObject>();
    b["title"] = p.alerts.banner.title; b["countText"] = p.alerts.banner.countText; b["sub"] = p.alerts.banner.sub;
    b["until"] = p.alerts.banner.until; b["next"] = p.alerts.banner.next;
    JsonArray items = a["items"].to<JsonArray>();
    for (auto& x : p.alerts.items) {
      JsonObject i = items.add<JsonObject>();
      i["event"] = x.event; i["nameZh"] = x.nameZh; i["priority"] = x.priority; i["vtecAction"] = x.vtecAction;
      i["inEffect"] = x.inEffect; i["periodText"] = x.periodText; i["areaText"] = x.areaText;
      JsonArray f = i["facts"].to<JsonArray>(); for (auto& s : x.facts) f.add(s);
      JsonArray c = i["chips"].to<JsonArray>(); for (auto& s : x.chips) c.add(s);
    }
    JsonArray pf = a["productFacts"].to<JsonArray>(); for (auto& s : p.alerts.productFacts) pf.add(s);
    JsonObject t = o["tide"].to<JsonObject>();
    t["ok"] = p.tide.ok; t["station"] = p.tide.station; t["rangeM"] = p.tide.rangeM; t["sunrise"] = p.tide.sunrise; t["sunset"] = p.tide.sunset;
    JsonArray ex = t["extremes"].to<JsonArray>();
    for (auto& e : p.tide.extremes) { JsonArray x = ex.add<JsonArray>(); x.add(e.minute); x.add(e.h); x.add(e.high ? "H" : "L"); }
    JsonObject tl = o["tiles"].to<JsonObject>();
    tile(tl["wind"].to<JsonObject>(), p.wind); tile(tl["wave"].to<JsonObject>(), p.wave);
    tile(tl["weather"].to<JsonObject>(), p.weather); tile(tl["water"].to<JsonObject>(), p.water);
    JsonArray ds = o["days"].to<JsonArray>();
    for (auto& dc : p.days) {
      JsonObject x = ds.add<JsonObject>();
      x["date"] = dc.date; x["day"] = dc.day; x["md"] = dc.md; x["icon"] = dc.icon; x["wx"] = dc.wx; x["temp"] = dc.temp;
      x["wind"] = dc.wind; x["row2Label"] = dc.row2Label; x["row2"] = dc.row2; x["tide"] = dc.tide; x["alert"] = dc.alert;
    }
  }
  std::string s;
  serializeJson(d, s);
  return s;
}

}  // namespace core
