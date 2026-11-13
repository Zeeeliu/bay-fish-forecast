// Network task (core 0): Wi-Fi via the ESP32-C6 (esp-hosted), NTP, fetch schedule (PRD §6.3), OTA (PRD §10.9).
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <time.h>
#include <algorithm>
#include "app.h"
#include "tz.h"
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""   // OTA stays off unless include/secrets.h sets a password of 8+ characters
#endif

static const char* UA = "(bay-fish-forecast, https://github.com/Zeeeliu/bay-fish-forecast)";   // NWS asks for an identifying User-Agent
static Preferences prefs;
static std::string credSsid, credPass;
static bool otaStarted = false;

bool wifiHaveCredentials() { Lock l; return !credSsid.empty(); }

static void loadCredentials() {
  prefs.begin("wifi", false);
  credSsid = prefs.getString("ssid", "").c_str();
  credPass = prefs.getString("pass", "").c_str();
#ifdef WIFI_SSID
  if (credSsid.empty() && strlen(WIFI_SSID)) { credSsid = WIFI_SSID; credPass = WIFI_PASS; }
#endif
}

static void startJoin() {
  // PRD §10.5: scan all channels and join the strongest node; 20 MHz; no persistence; auto-reconnect
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  WiFi.disconnect(false, false);
  WiFi.begin(credSsid.c_str(), credPass.empty() ? nullptr : credPass.c_str());
  Lock l; app.wifiSsid = credSsid; app.wifiStatus = "连接中…";
}

// ---------- HTTPS ----------
static bool httpGet(const std::string& url, std::string& body, String& err) {
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();   // PRD §10.2: validate certificates with the built-in Mozilla bundle; never setInsecure()
  HTTPClient http;
  http.setTimeout(10000);
  http.setConnectTimeout(10000);
  if (!http.begin(client, url.c_str())) { err = "begin failed"; return false; }
  http.setUserAgent(UA);
  bool nws = url.find("api.weather.gov") != std::string::npos;
  bool zoneFc = url.find("/zones/forecast/") != std::string::npos;   // JSON-LD: 36 KB instead of 154 KB of zone geometry
  http.addHeader("Accept", zoneFc ? "application/ld+json" : nws ? "application/geo+json" : "*/*");
  int code = http.GET();
  if (code != 200) { err = String("HTTP ") + code + " " + http.errorToString(code); http.end(); return false; }
  String s = http.getString();
  http.end();
  if (s.isEmpty()) { err = "empty body"; return false; }
  body.assign(s.c_str(), s.length());
  return true;
}

static bool fetchKey(const std::string& key, const std::string& url) {
  if (otaStarted) ArduinoOTA.handle();   // stay reachable for OTA during long refreshes
  std::string body; String err;
  bool ok = httpGet(url, body, err);
  if (!ok) {   // PRD §6.3: one retry after 30 s; keep answering OTA while waiting
    for (int i = 0; i < 300; i++) { if (otaStarted) ArduinoOTA.handle(); delay(100); }
    ok = httpGet(url, body, err);
  }
  core::Fetched snapshot;
  {
    Lock l;
    auto& f = app.inputs.src[key];
    f.lastOk = ok;
    if (ok) { f.body = std::move(body); f.okAt = nowUtc(); }
    else app.lastError = key + ": " + err.c_str();
    snapshot = f;
  }
  if (ok) storeSave(key, snapshot);
  DBG("fetch %-14s %s %s\n", key.c_str(), ok ? "ok" : "FAIL", ok ? "" : err.c_str());   // behind `debug`: never interleave with JSON replies
  return ok;
}

static std::vector<std::string> marineZones() {   // unique marine zones of all places, in table order
  std::vector<std::string> z;
  for (auto& p : core::places()) if (std::find(z.begin(), z.end(), p.marineZone) == z.end()) z.push_back(p.marineZone);
  return z;
}

static std::string ll(double v) { char b[16]; snprintf(b, sizeof b, "%.4f", v); return b; }

static void alertCycle() {
  for (auto& p : core::places())
    fetchKey(std::string("S1_") + p.id, "https://api.weather.gov/alerts/active?point=" + ll(p.landLat) + "," + ll(p.landLon));
  for (auto& z : marineZones()) fetchKey("S2_" + z, "https://api.weather.gov/alerts/active?zone=" + z);
  app.lastAlertCycle = nowUtc();
}

static std::string ymd(int64_t utc) { core::LocalTime t = core::toLocal(utc); char b[12]; snprintf(b, sizeof b, "%04d%02d%02d", t.y, t.mo, t.d); return b; }

static void fullRefresh() {
  { Lock l; app.inputs.refreshing = true; }
  app.dataVersion++;
  alertCycle();
  for (auto& p : core::places()) {
    fetchKey(std::string("S3_") + p.id, "https://api.open-meteo.com/v1/forecast?latitude=" + ll(p.fcLat) + "&longitude=" + ll(p.fcLon) +
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant,sunrise,sunset"
             "&current=temperature_2m,wind_speed_10m,wind_gusts_10m,wind_direction_10m,weather_code&wind_speed_unit=kn&timezone=America/Los_Angeles&forecast_days=6");
    if (p.profile == core::Profile::Coastal)
      fetchKey(std::string("S4_") + p.id, "https://marine-api.open-meteo.com/v1/marine?latitude=" + ll(p.mrLat) + "&longitude=" + ll(p.mrLon) +
               "&daily=wave_height_max,wave_period_max,wave_direction_dominant&current=wave_height,wave_period,sea_surface_temperature&timezone=America/Los_Angeles&forecast_days=6");
    int64_t now = nowUtc();
    fetchKey(std::string("S5_") + p.tideStation, std::string("https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=predictions&application=dad_fish_display&station=") +
             p.tideStation + "&begin_date=" + ymd(now - 86400) + "&end_date=" + ymd(now + 5 * 86400) + "&datum=MLLW&time_zone=lst_ldt&interval=hilo&units=metric&format=json");
    if (*p.buoy) fetchKey(std::string("S6_") + p.buoy, std::string("https://www.ndbc.noaa.gov/data/latest_obs/") + p.buoy + ".txt");
  }
  for (auto& z : marineZones())   // S8: official marine zone forecast (wind, gusts, seas)
    fetchKey("S8_" + z, "https://api.weather.gov/zones/forecast/" + z + "/forecast");
  bool s7 = fetchKey(std::string("S7_") + core::kBayWaterStation, std::string("https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=water_temperature&date=latest&station=") +
                     core::kBayWaterStation + "&units=metric&time_zone=lst_ldt&format=json&application=dad_fish_display");
  if (!s7) for (auto& p : core::places()) if (p.profile == core::Profile::Bay) {   // PRD §6.3: bay SST fallback only when S7 fails
    fetchKey(std::string("S4_") + p.id, "https://marine-api.open-meteo.com/v1/marine?latitude=" + ll(p.mrLat) + "&longitude=" + ll(p.mrLon) +
             "&current=sea_surface_temperature&timezone=America/Los_Angeles&forecast_days=6");
  }
  app.lastFullRefresh = nowUtc();
  { Lock l; app.inputs.refreshing = false; app.inputs.lastFullRefreshAt = app.lastFullRefresh; }
}

// ---------- schedule ----------
static int64_t nextAlertAt(int64_t after) {   // :02 :17 :32 :47 + jitter
  core::LocalTime t = core::toLocal(after);
  for (int add = 0; add < 120; add++) {
    int m = (t.mi + add) % 60;
    if (m % 15 == 2) {
      int64_t c = (after / 60 + add) * 60 + (int64_t)random(0, 60);
      if (c > after) return c;
    }
  }
  return after + 900;
}
static int64_t nextFullAt(int64_t after) {
  for (int d = 0; d < 2; d++) {
    core::LocalTime t = core::toLocal(after + d * 86400);
    for (int h : {5, 12, 18}) {
      int64_t c = core::fromLocal(t.y, t.mo, t.d, h, 0, 0) + random(0, 60);
      if (c > after) return c;
    }
  }
  return after + 6 * 3600;
}

static void doScan() {
  app.scanning = true;
  int n = WiFi.scanNetworks(false, false, false, 300);
  std::vector<WifiNet> out;
  for (int i = 0; i < n; i++) {
    std::string s = WiFi.SSID(i).c_str();
    if (s.empty()) continue;
    bool dup = false;
    for (auto& o : out) if (o.ssid == s) { dup = true; o.rssi = std::max(o.rssi, (int)WiFi.RSSI(i)); }
    if (!dup) out.push_back({s, (int)WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
  }
  WiFi.scanDelete();
  std::sort(out.begin(), out.end(), [](const WifiNet& a, const WifiNet& b) { return a.rssi > b.rssi; });
  { Lock l; app.scan = out; }
  app.scanning = false;
}

static void netTask(void*) {
  int64_t alertDue = 0, fullDue = 0;   // 0 = run as soon as online
  uint32_t joinStart = 0;
  bool wasConnected = false, timeRequested = false;
  for (;;) {
    if (app.scanReq) { app.scanReq = false; doScan(); }
    if (app.connectReq) {
      std::string s, p;
      { Lock l; s = app.reqSsid; p = app.reqPass; app.connectReq = false; }
      credSsid = s; credPass = p;
      prefs.putString("ssid", s.c_str()); prefs.putString("pass", p.c_str());
      startJoin(); joinStart = millis();
    }
    bool conn = WiFi.status() == WL_CONNECTED;
    if (conn != wasConnected) {
      wasConnected = conn;
      app.wifiConnected = conn;
      { Lock l; app.inputs.wifiConnected = conn; app.wifiStatus = conn ? "已连接" : "未连接"; }
      Serial.printf("wifi: %s %s rssi=%d\n", conn ? "connected" : "disconnected", WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
      if (conn) {
        WiFi.setSleep(false);
        if (!timeRequested) { configTzTime("PST8PDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.google.com"); timeRequested = true; }
        if (!otaStarted && strlen(OTA_PASSWORD) >= 8) { ArduinoOTA.setHostname("bay-fish-forecast"); ArduinoOTA.setPassword(OTA_PASSWORD); ArduinoOTA.begin(); otaStarted = true; }
      }
      app.dataVersion++;
    }
    if (!conn && !credSsid.empty() && (joinStart == 0 || millis() - joinStart > 20000)) {
      if (joinStart && WiFi.status() == WL_CONNECT_FAILED) { Lock l; app.wifiStatus = "连接失败，请检查密码"; }
      startJoin(); joinStart = millis();
    }
    if (conn) {
      if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) clockOnNtp();   // also on SNTP's periodic re-syncs
      if (otaStarted) ArduinoOTA.handle();
      int64_t now = nowUtc();
      if (app.clockValid) {
        if (app.refreshReq || fullDue == 0 || now >= fullDue) {
          app.refreshReq = false;
          fullRefresh();
          now = nowUtc();
          fullDue = nextFullAt(now);
          alertDue = nextAlertAt(now);
          app.dataVersion++;
        } else if (app.alertReq || alertDue == 0 || now >= alertDue) {
          app.alertReq = false;
          alertCycle();
          alertDue = nextAlertAt(nowUtc());
          app.dataVersion++;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void netBegin() {
  loadCredentials();
  // Tab5: the ESP32-C6 radio is on SDIO2 (CLK 12, CMD 13, D0 11, D1 10, D2 9, D3 8, RST 15)
  WiFi.setPins(GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_11, GPIO_NUM_10, GPIO_NUM_9, GPIO_NUM_8, GPIO_NUM_15);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  if (!credSsid.empty()) startJoin();
  else { Lock l; app.wifiStatus = "未设置 Wi-Fi"; }
  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
}
