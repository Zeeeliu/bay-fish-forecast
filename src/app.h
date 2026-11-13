// Shared state between the network task (core 0) and the UI loop (core 1).
#pragma once
#include <Arduino.h>
#include <string>
#include <vector>
#include "model.h"

#define FW_VERSION "1.0.1"

struct WifiNet { std::string ssid; int rssi; bool secure; };

struct AppState {
  SemaphoreHandle_t mu;
  core::Inputs inputs;               // guarded by mu
  volatile uint32_t dataVersion = 0; // bumped after every fetch cycle
  // Wi-Fi (net task owns the radio; UI only posts requests)
  volatile bool wifiConnected = false;
  std::string wifiSsid, wifiStatus = "未连接";     // guarded by mu
  std::vector<WifiNet> scan;                        // guarded by mu
  volatile bool scanning = false, scanReq = false, connectReq = false, refreshReq = false, alertReq = false;
  std::string reqSsid, reqPass;                     // guarded by mu
  volatile bool clockValid = false, ntpSynced = false;
  volatile bool debug = false;
  std::string lastError;                            // guarded by mu
  std::vector<std::string> errorLog;                // guarded by mu; last 12 failures "MM-DD HH:MM key: reason"
  std::vector<std::string> retryKeys;               // guarded by mu; forecast sources waiting for a catch-up retry
  int64_t lastAlertCycle = 0, lastFullRefresh = 0;
};
extern AppState app;

struct Lock {
  Lock() { xSemaphoreTake(app.mu, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(app.mu); }
};

// net.cpp
void netBegin();
bool wifiHaveCredentials();
// store.cpp
void storeBegin();
void storeLoad(core::Inputs& in);
void storeSave(const std::string& key, const core::Fetched& f);
// clock.cpp
void clockBegin();
void clockOnNtp();
int64_t nowUtc();
// ui.cpp
void uiBegin();
void uiLoop();
// serial_cmd.cpp
void serialLoop();

#define DBG(...) do { if (app.debug) Serial.printf(__VA_ARGS__); } while (0)
