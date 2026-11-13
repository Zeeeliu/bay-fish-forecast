// Raw-response cache in LittleFS (PRD §10.6): /cache/<key>.txt + /cache/meta.json, atomic writes (tmp + rename).
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "app.h"

static bool fsOk = false;

void storeBegin() {
  fsOk = LittleFS.begin(false);
  if (!fsOk) { Serial.println("store: LittleFS mount failed (run `pio run -t uploadfs`); formatting"); fsOk = LittleFS.begin(true); }
  if (fsOk && !LittleFS.exists("/cache")) LittleFS.mkdir("/cache");
  Serial.printf("store: LittleFS %s, used %u / %u\n", fsOk ? "ok" : "FAILED", (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
}

static bool writeAtomic(const String& path, const uint8_t* data, size_t n) {
  String tmp = path + ".tmp";
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  bool ok = f.write(data, n) == n;
  f.close();
  if (!ok) { LittleFS.remove(tmp); return false; }
  LittleFS.remove(path);
  return LittleFS.rename(tmp, path);
}

static void saveMeta(const core::Inputs& in) {
  JsonDocument d;
  for (auto& kv : in.src) d[kv.first]["okAt"] = kv.second.okAt;
  String s; serializeJson(d, s);
  writeAtomic("/cache/meta.json", (const uint8_t*)s.c_str(), s.length());
}

void storeSave(const std::string& key, const core::Fetched& f) {
  if (!fsOk) return;
  writeAtomic(String("/cache/") + key.c_str() + ".txt", (const uint8_t*)f.body.data(), f.body.size());
  core::Inputs copy;
  { Lock l; copy.src = app.inputs.src; }
  for (auto& kv : copy.src) kv.second.body.clear();
  saveMeta(copy);
}

void storeLoad(core::Inputs& in) {
  if (!fsOk) return;
  File m = LittleFS.open("/cache/meta.json", "r");
  if (!m) return;
  JsonDocument d;
  if (deserializeJson(d, m)) { m.close(); return; }
  m.close();
  int n = 0;
  for (JsonPair kv : d.as<JsonObject>()) {
    File f = LittleFS.open(String("/cache/") + kv.key().c_str() + ".txt", "r");
    if (!f) continue;
    core::Fetched x;
    x.body.resize(f.size());
    f.read((uint8_t*)&x.body[0], x.body.size());
    f.close();
    x.okAt = kv.value()["okAt"] | 0;
    x.lastOk = true;   // treated as good until the next attempt says otherwise
    in.src[kv.key().c_str()] = std::move(x);
    n++;
  }
  Serial.printf("store: loaded %d cached responses\n", n);
}
