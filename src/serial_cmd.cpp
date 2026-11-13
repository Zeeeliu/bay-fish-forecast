// USB serial diagnostics (PRD §10.8): one JSON command per line -> one JSON reply line.
#include <ArduinoJson.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_system.h>
#include "app.h"
#include "tz.h"

std::string uiReportJson();
void uiScreenshot();
void uiTap(int x, int y, bool hold);
#include <sys/time.h>
static String lineBuf;

static const char* resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power-on"; case ESP_RST_SW: return "software"; case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "watchdog"; case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_USB: return "usb"; default: return "other";
  }
}

static void reply(JsonDocument& d) { serializeJson(d, Serial); Serial.println(); }

static void handle(const String& line) {
  JsonDocument in;
  String cmd = line;
  if (line.startsWith("{")) { if (deserializeJson(in, line)) return; cmd = in["cmd"] | ""; }
  cmd.trim();
  JsonDocument out;
  out["ok"] = true; out["cmd"] = cmd;
  if (cmd == "status") {
    JsonObject d = out["data"].to<JsonObject>();
    d["fw_version"] = FW_VERSION; d["uptime_s"] = millis() / 1000; d["reset_reason"] = resetName(esp_reset_reason());
    d["heap_free"] = ESP.getFreeHeap(); d["psram_free"] = ESP.getFreePsram();
    JsonObject w = d["wifi"].to<JsonObject>();
    { Lock l; w["ssid"] = app.wifiSsid; w["status"] = app.wifiStatus; d["last_error"] = app.lastError;
      JsonArray el = d["error_log"].to<JsonArray>(); for (auto& e : app.errorLog) el.add(e);
      JsonArray rk = d["retry_pending"].to<JsonArray>(); for (auto& k : app.retryKeys) rk.add(k); }
    w["connected"] = (bool)app.wifiConnected; w["rssi"] = WiFi.RSSI(); w["ip"] = WiFi.localIP().toString();
    d["clock_valid"] = (bool)app.clockValid; d["ntp_synced"] = (bool)app.ntpSynced;
    d["now"] = core::localDate(nowUtc()) + " " + core::hhmm(nowUtc());
    d["last_alert_cycle"] = app.lastAlertCycle ? core::hhmm(app.lastAlertCycle) : "";
    d["last_full_refresh"] = app.lastFullRefresh ? core::hhmm(app.lastFullRefresh) : "";
    JsonObject src = d["sources"].to<JsonObject>();
    { Lock l; for (auto& kv : app.inputs.src) { JsonObject s = src[kv.first].to<JsonObject>(); s["ok"] = kv.second.lastOk; s["at"] = kv.second.okAt ? core::hhmm(kv.second.okAt) : ""; s["bytes"] = kv.second.body.size(); } }
    JsonObject p = d["power"].to<JsonObject>();
    p["vbus_mv"] = M5.Power.getVBUSVoltage(); p["battery_mv"] = M5.Power.getBatteryVoltage(); p["battery_pct"] = M5.Power.getBatteryLevel();
  } else if (cmd == "report") {
    Serial.println(uiReportJson().c_str());
    return;
  } else if (cmd == "shot") { uiScreenshot(); return; }
  else if (cmd == "tap") uiTap(in["x"] | 0, in["y"] | 0, in["hold"] | false);
  else if (cmd == "settime") {   // testing: set the clock (UTC epoch) and the RTC
    timeval tv = {(time_t)(in["epoch"] | 0LL), 0}; settimeofday(&tv, nullptr);
    struct tm g; time_t t = tv.tv_sec; gmtime_r(&t, &g); if (M5.Rtc.isEnabled()) M5.Rtc.setDateTime(&g);
    app.clockValid = t > 1735689600; app.dataVersion++;
  } else if (cmd == "fetch") app.refreshReq = true;
  else if (cmd == "alerts") app.alertReq = true;
  else if (cmd == "debug") app.debug = in["on"] | false;
  else if (cmd == "wifi_set") {
    Lock l; app.reqSsid = (const char*)(in["ssid"] | ""); app.reqPass = (const char*)(in["password"] | ""); app.connectReq = true;
  } else if (cmd == "reboot") { reply(out); Serial.flush(); delay(200); ESP.restart(); }
  else { out["ok"] = false; out["error"] = "unknown command (status, report, fetch, alerts, debug, wifi_set, reboot)"; }
  reply(out);
}

void serialLoop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { if (lineBuf.length()) handle(lineBuf); lineBuf = ""; }
    else if (lineBuf.length() < 1024) lineBuf += c;
  }
}
