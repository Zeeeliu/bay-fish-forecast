# PRD — 钓鱼海况屏 (Fishing Conditions Display) for M5Stack Tab5

| | |
|---|---|
| **Primary user** | A Chinese-reading angler who fishes from shore and boat in the SF Bay Area |
| **Hardware** | M5Stack Tab5 (ESP32-P4 + ESP32-C6 Wi-Fi, 5" 1280×720 touch). Probed over USB, see §10.1 |
| **Architecture** | Device-only: the Tab5 fetches public APIs directly. No server, no Google Apps Script (§5) |
| **UI baseline** | **Prototype 2** (tide-chart layout) in the design mockup (§9) |
| **Status** | **v3.5 — implemented**, running on a Tab5 since 2026-11-12 (code: this repository, see Appendix C) |
| **Design** | Screenshots of the running device: `docs/images/`. The design was prototyped as "Prototype 2" (tide-chart layout); "Prototype 1" is superseded. |

Conventions:
- **MUST / SHOULD / MAY** are used in the RFC 2119 sense.
- **✅ verified** = confirmed live on 2026-11-11/12, against the real APIs or this Tab5.
- **⚠ VERIFY** = an assumption to confirm at the milestone named (§16 lists them all).
- **📎** = patterns reused from an earlier private M5Stack project ("mochi", not public) (§17.1).

---

## 0. Decision Log

| # | Decision | Rationale | Date |
|---|---|---|---|
| D1 | **Device-only architecture.** The Tab5 calls NWS, NOAA and Open-Meteo directly. | Every response is ≤ 33 KB (✅ measured); the device controls the User-Agent NWS requires; one codebase; no Google account or redirect. Alternatives are in Appendix A. | 2026-11-11 |
| D2 | **UI = Prototype 2**: compact header and banner, today's tide chart, 2×2 condition tiles, 5-day cards. | The user decided to build from Prototype 2. | 2026-11-12 |
| D3 | **The tide chart shows no "now" marker.** It is static for the day and redraws only when data changes. | User request; avoids a redraw every minute. | 2026-11-12 |
| D4 | **Chinese alert text comes from curated templates plus extracted facts, not machine translation.** The English original is always available. | NWS descriptions bundle several events into one text (✅ seen live), which garbles machine translation; reviewed wording is safer. An LLM is an optional add-on (Appendix A.3). | 2026-11-11 |
| D5 | **Numeric weather from Open-Meteo, official hazards only from NWS.** | Open-Meteo gives structured knots/°C; NWS gives wind as text. Only NWS can set the alert state. | 2026-11-11 |
| D6 | **Full forecast refresh 3×/day (05:00/12:00/18:00), alert check every 15 min.** | Original requirement plus fast hazard updates; ≈530 requests/day. | 2026-11-11 |
| D7 | **Default units: 米, °C, 节**, with 英尺 shown as secondary for waves. | Confirmed Celsius on 2026-11-12. All temperatures are labelled **°C** (e.g. `12–21°C`), never a bare `°`, to avoid Fahrenheit confusion. | 2026-11-11 |
| D11 | **English option for the landing page.** A `EN` / `中文` pill after the location tabs switches S1 (tabs, banner, tide chart, tiles, 5-day cards, footer) between Chinese and English; the choice is saved in NVS. The alert detail page (S2) stays Chinese, with the English original. Default: Chinese. | User request. The core builds both a Chinese and an English Report on every rebuild, so switching is instant and S2 always has Chinese text. | 2026-11-12 |
| D10 | **Fourth location: Santa Cruz (圣克鲁斯)**, using the same sources and setup (coastal profile). All IDs ✅ verified live on 2026-11-12; CO-OPS 9413745 has no water temperature, so it comes from buoy 46236. | User request. Monterey Bay had a live Gale Warning (gusts to 50 kt) when it was added. | 2026-11-12 |
| D9 | **Wind, gusts and seas on the 5-day cards come from the NWS marine zone forecast** (PZZ545 coast, PZZ531 bay). Open-Meteo stays for air temperature, weather and the current wind, and is the fallback for days NWS doesn't cover. Revises D5. | The accuracy audit on 2026-11-12 found Open-Meteo gusts of ≈ 24 kt on Friday where the official marine forecast said S 25–30 kt, gusts to 40 kt. For boating decisions, the official forecast must win. | 2026-11-12 |
| D8 | **The condition tiles show the time of their data** (e.g. `风 · 05:00 实况`) instead of the prototype's `现在`. | Those values change only at the 3 daily refreshes. Calling them "now" in the afternoon would be misleading. | 2026-11-12 |

---

## 1. Problem & Goals

The user decides when and where to fish based on waves, tide, wind and weather. He also needs to know whether any **official hazard statement** is in effect, especially an NWS *Beach Hazards Statement*. This information is spread across English-language US government sites.

**Goal:** an always-on, glanceable Simplified-Chinese display. For four locations it shows the official alert status, today's tide curve, today's conditions and a 5-day outlook, all updated automatically, with nothing to maintain.

### 1.1 Goals
- **G1** Official NWS hazard status per location, with **no ambiguity** between "alert issued", "no alert found" and "status unknown".
- **G2** Key fishing data: today's tide curve with highs/lows, wind and gusts, waves and swell period, air temperature, water temperature, weather. Today plus 5 days.
- **G3** Fully Simplified-Chinese UI that an older adult can read at arm's length (validated with the user in M2).
- **G4** Fully automatic: forecast refresh 3×/day, alert re-check every 15 min, sensible behavior offline.
- **G5** No server and no paid services in v1.

### 1.2 Non-goals (v1)
- Fish activity or bite predictions, solunar tables, or any "safe to go" score. **The product never declares conditions safe.**
- Fishing regulations / season dates (CDFW) and shellfish/domoic acid notices (CDPH). Candidate for v2.
- Phone notifications (v2, Appendix A.4).
- Machine translation of alert prose (Appendix A.3).
- Traditional Chinese (easy later via the string tables).
- Multi-device management. (Wi-Fi *is* configurable on the device: a touch keyboard screen, added during implementation; see Appendix C.)

---

## 2. User & Usage

- The device sits on a shelf or kitchen counter on USB-C power and is always on. The installed battery acts as a backup (§10.1).
- The user glances at it in the morning (≈ 05:00–07:00) and in the evening before deciding on a trip.
- Interactions:
  - tap a location tab
  - tap the alert banner to open the alert details
  - tap back
  - long-press the date/status area to force a refresh
- The user never configures anything.

---

## 3. Scope (v1 feature list)

| # | Feature | Spec |
|---|---|---|
| F1 | Four locations, switchable by tab: 半月湾 / 旧金山 / 红木城 / 圣克鲁斯 (D10) | §4, §9.3 |
| F2 | Official alert banner per location: three states, in-effect vs upcoming, count, extracted fact | §7.1, §9.3 |
| F3 | Alert detail screen: per-alert Chinese summary, facts, keyword chips, English original | §7.2, §9.5 |
| F4 | Today's tide chart: curve, highs/lows, sunrise/sunset shading, tidal range | §7.5, §9.4 |
| F5 | Condition tiles: wind, waves (or bay conditions), weather/temperature, water temperature | §7.4, §9.3 |
| F6 | 5-day forecast cards with alert tags | §7.4, §7.1.5, §9.3 |
| F7 | Automatic refresh, offline cache, staleness rules | §6.3, §7.6, §10 |
| F8 | Serial diagnostics and OTA updates | §10.8, §10.9 |

---

## 4. Locations & Data Mapping (✅ all IDs verified live)

Each location has a **profile**. Offshore wave data must never be shown as a bay location's local waves.

| id | 中文名 | Profile | Land point (NWS `/points` + alert point query) | NWS grid / zone / county | Marine zone | Tide station | Obs / water temp | Open-Meteo forecast point | Open-Meteo marine point |
|---|---|---|---|---|---|---|---|---|---|
| `halfmoon` | 半月湾 | `coastal` | 37.4636, -122.4286 | MTR 82,92 / CAZ509 / CAC081 | PZZ545 | 9414131 Pillar Point Harbor | NDBC 46012 | 37.4636, -122.4286 | 37.48, -122.52 |
| `sf` | 旧金山 | `coastal` (Ocean Beach) | 37.7600, -122.5100 | MTR 81,105 / CAZ006 / CAC075 | PZZ545 | 9414290 San Francisco | NDBC 46026 | 37.7600, -122.5100 | 37.76, -122.54 |
| `santacruz` | 圣克鲁斯 | `coastal` (Monterey Bay) | 36.9690, -122.0230 | MTR 91,67 / **CAZ529** / CAC087 | **PZZ535** (Monterey Bay) | ✅ **9413745** Santa Cruz, Monterey Bay (secondary station, hi/lo only) | NDBC **46236** (Monterey Canyon, in the bay; seas + water temp) | 36.9690, -122.0230 | 36.93, -122.03 |
| `redwood` | 红木城 | `bay` | 37.4852, -122.2364 | MTR 88,91 / CAZ508 / CAC081 | PZZ531 | 9414523 Redwood City | CO-OPS 9414750 Alameda (water temp) | 37.4852, -122.2364 | 37.55, -122.22 (SST fallback only) |

Verified gotchas:
- **`/points` needs a land coordinate.** Coordinates in the water (e.g. at the tide stations) return no `properties`.
- A *water* point inside a marine zone makes `alerts/active?point=` return marine alerts. Don't rely on that: query marine zones explicitly.
- The far south bay (≈ 37.52–37.55 N) has **no marine zone**; PZZ531 starts further north. Use PZZ531 explicitly for `redwood`.
- **Bay locations do get alerts.** On 2026-11-11/12 the Redwood City point had a Coastal Flood Advisory and PZZ531 had a Small Craft Advisory.

Profile rules:
- `coastal`: the wave tile shows offshore wave height (m + ft) and period; the 5-day card's second row is `浪`.
- `bay`: no wave height anywhere. The wave tile becomes `湾内水况` (estimated from wind, §7.4); the 5-day second row is `潮差` (daily tidal range).
- Each location uses only its own alert sources (its land point + its marine zone). Alerts are never copied between locations.

Future (not v1): SF bay-side profile (e.g. Fort Point / Pier 7) as a 4th location with `bay` rules.

---

## 5. Architecture

```
┌──────────────────────────────── M5Stack Tab5 ────────────────────────────────┐
│ net task (core 0, background)                                                 │
│   alert cycle  every 15 min ......... NWS alerts × 5                          │
│   full refresh 05:00/12:00/18:00, boot, manual ... all sources, 16 GETs        │
│   HTTPS GET (CA bundle, User-Agent) → raw text → core                         │
│                                                                               │
│ core  (portable C++17: no Arduino, network, display or clock calls)           │
│   parse_nws · parse_openmeteo · parse_coops · parse_ndbc                      │
│   alerts      filter, dedupe, in-effect/upcoming, priority, state, day tags  │
│   alert_text  sections, per-event clauses, facts, keyword chips              │
│   tide        extremes → curve samples, daily range, highs/lows              │
│   format_zh   Chinese strings, units, templates                              │
│   → Report (in-memory model, §8; rebuilt as time passes, §7.7)               │
│                                                                               │
│ store  raw responses + fetch times in LittleFS (atomic tmp+rename) = cache   │
│ ui     draws Report with M5GFX; the same code runs in the SDL emulator        │
│ clock  NTP → RX8130 RTC; TZ PST8PDT,M3.2.0,M11.1.0                            │
│ serial status / fetch / debug / reboot      ota  ArduinoOTA on the LAN        │
└──────────────────────────────────────────────────────────────────────────────┘
```

**Key rule:** `core` takes response strings and a `now` timestamp as inputs and returns the `Report`. All alert, tide and formatting logic is therefore unit-testable on the Mac against the real captured responses in `fixtures/`.

### 5.1 Repository layout
```
fish-display/
  platformio.ini         envs: board_Tab5 (§10.2), emulator_Tab5 (SDL), native (unit tests)
  lib/core/              model.h, parse_*.cpp, alerts.cpp, alert_text.cpp, tide.cpp, format_zh.cpp
  src/                   main.cpp, net.cpp, store.cpp, clock.cpp, ui.cpp, ui_chart.cpp, serial_cmd.cpp, ota.cpp
  emulator/              SDL main: fixtures/ → core → the same ui.cpp
  test/                  native tests: fixtures/ → core → expected Report fields and strings
  fixtures/              real API responses (captured live on 2026-11-11/12)
  include/secrets.example.h      (secrets.h is gitignored)
  README.md              build, flash, OTA, restore-factory steps
```

---

## 6. Data Sources & Fetch Plan

All sources are free and need no key. Every request sends `User-Agent: (bay-fish-forecast, https://github.com/Zeeeliu/bay-fish-forecast)` (NWS requires an identifying UA; ✅ a custom UA was accepted). NWS requests also send `Accept: application/geo+json`.

### 6.1 Endpoints

| # | Data | Request | Size (✅ measured) |
|---|---|---|---|
| S1 | Alerts, land | `https://api.weather.gov/alerts/active?point={land lat},{lon}` | 4.7 KB (1 alert) – 33 KB (4 alerts) |
| S2 | Alerts, marine | `https://api.weather.gov/alerts/active?zone={PZZ545\|PZZ531}` | ≈ 5 KB |
| S3 | Forecast + current | `https://api.open-meteo.com/v1/forecast?latitude=..&longitude=..&daily=weather_code,temperature_2m_max,temperature_2m_min,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant,sunrise,sunset&current=temperature_2m,wind_speed_10m,wind_gusts_10m,wind_direction_10m,weather_code&wind_speed_unit=kn&timezone=America/Los_Angeles&forecast_days=6` | ≈ 1.4 KB |
| S4 | Waves (coastal); SST fallback (bay, only when S7 fails) | `https://marine-api.open-meteo.com/v1/marine?latitude=..&longitude=..&daily=wave_height_max,wave_period_max,wave_direction_dominant&current=wave_height,wave_period,sea_surface_temperature&timezone=America/Los_Angeles&forecast_days=6` | ≈ 0.8 KB |
| S5 | Tides (hi/lo) | `https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=predictions&application=dad_fish_display&station={id}&begin_date={yesterday yyyyMMdd}&end_date={today+5 yyyyMMdd}&datum=MLLW&time_zone=lst_ldt&interval=hilo&units=metric&format=json` | ≈ 1.5 KB |
| S6 | Buoy obs (coastal) | `https://www.ndbc.noaa.gov/data/latest_obs/{46012\|46026}.txt` | ≈ 0.3 KB |
| S8 | Marine zone forecast (wind, gusts, seas) | `https://api.weather.gov/zones/forecast/{PZZ545\|PZZ531}/forecast` with `Accept: application/ld+json` (36 KB; geo+json is 154 KB of zone geometry). Periods have only names ("Thu", "Fri Night"); map them to dates from `updated` | ≈ 36 KB |
| S7 | Bay water temp | `https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=water_temperature&date=latest&station=9414750&units=metric&time_zone=lst_ldt&format=json&application=dad_fish_display` | ≈ 0.2 KB |

Do **not** use: `alerts/active?area=CA` (178 KB), NWS raw `gridpoints/{wfo}/{x},{y}` (254 KB), NDBC `realtime2/*.txt` (100–600 KB).

S5 asks for **yesterday through today+5** so the chart's curve is continuous at 00:00 and 24:00 (it needs the extremes on both sides of midnight), and so the 5 day cards are covered.
S3/S4 ask for **6 days**. After local midnight, and before the 05:00 refresh, the first day of the cached data is "yesterday"; the spare day keeps 5 cards starting today (§7.7).

### 6.2 Source quirks (✅ observed)
- **NWS alerts:**
  - One product can bundle several events. Each event is its own feature, but `description` holds the **combined** text (§7.2).
  - `alerts/active` also returns alerts whose `onset` is in the **future**. Example: a High Surf Warning starting Fri 18:00 was "active" on Thu.
  - `parameters.VTEC[0]` (e.g. `/O.CON.KMTR.BH.S.0024.000000T0000Z-261114T0200Z/`) gives the action code: NEW / CON / EXT / CAN / EXP.
- **NDBC `latest_obs`:**
  - Imperial units (`Seas: 6.6 ft`, `Wind: SE (140°), 3.9 kt`, `Water Temp: 66.0 °F`).
  - The `°` sign is a single **Latin-1 byte 0xB0**.
  - The observation time is the first line matching `h:mm am|pm PST|PDT mm/dd/yy`.
  - **Every field is optional, and the set changes between reports from the same buoy.** 46012 gave `Seas:` + `Peak Period:`. 46026 once gave only a `Wave Summary` block (`Swell:`, `Period:`, `Direction:`, `Wind Wave:`); an hour later it gave `Seas:` + `Peak Period:` **and** the summary, with no `Wind:` and no `Water Temp:`. Parse by key; never by line position.
  - ✅ Just after midnight the local time line has **no date** (`12:00 am PST`). Take the observation time from the `HHMM GMT mm/dd/yy` line, which is always complete.
- **CO-OPS:**
  - Water temperature is **not offered** at 9414131, 9414523 or 9414290 ("No data was found"). It is offered at 9414750 Alameda and 9414863 Richmond.
  - Tide values are strings in metres (`"v":"-0.141"`).
- **Open-Meteo:** `current` is the model value at the last quarter hour (`current.time`).

### 6.3 Schedule

| Cycle | When | Requests | Count |
|---|---|---|---|
| **Alert cycle** | Every 15 min at :02/:17/:32/:47, plus 0–60 s random jitter | S1 × 4 (one per place), S2 × 3 (one per distinct marine zone) | 7 |
| **Full refresh** | 05:00, 12:00, 18:00 local (+ jitter); on boot; on manual long-press | Alert cycle + S3 × 4 + S4 × 3 + S5 × 4 + S6 × 3 + S7 × 1 + S8 × 3 | 25 (+1 S4 at the bay point only if S7 fails) |

- Load ≈ 96 × 7 + 3 × 25 ≈ **750 requests/day** from one home IP.
- Per request: 10 s timeout, one retry after 30 s.
- A failure only affects its own section, which keeps its last good data and is marked stale (§7.6).
- Cycles never overlap. A due **full refresh** waits for a running alert cycle to finish, then runs. A due **alert cycle** is skipped while a full refresh is running, because the full refresh includes it.
- All networking runs in a background task. The UI never blocks; it shows `正在更新…` in the status area during a full refresh.

---

## 7. Domain Logic (`lib/core`)

### 7.1 Alert pipeline

**7.1.1 Collect and filter (per location):**
1. Merge the features from the location's S1 + S2 responses.
2. Deduplicate by `id`.
3. Keep only alerts with `status == "Actual"` and `messageType != "Cancel"`, and whose VTEC action is not `CAN`/`EXP`. Alerts **without** a VTEC string (some NWS products have none) are kept.
4. Drop alerts that have ended: `(ends ?? expires) <= now`.
5. **Unmapped event names are kept**, not dropped (fail open), and get priority 40.

**7.1.2 Classify each alert:**
- **in effect:** `(onset ?? effective) <= now < (ends ?? expires)`
- **upcoming:** `(onset ?? effective) > now`

**7.1.3 Location alert state:**
```
if any alert request for this location failed in the latest cycle, or
   the last successful check for this location is more than 45 min old, or
   the clock is not valid:                      state = UNKNOWN
elif the filtered alert list is non-empty:      state = ISSUED
else:                                           state = NONE
```
UNKNOWN never discards the cached alert list. The detail screen can still show it under the notice `以下为 HH:mm 的缓存信息，当前状态未知` (§9.5).

**7.1.4 Banner content (state ISSUED):**
- **Top alert** = the highest-priority alert *in effect*. If none is in effect, it's the upcoming alert with the earliest onset.
- **Count** = all filtered alerts for the location (in effect + upcoming). Shown as `共N条` when N ≥ 2.
- **Next chip** = the **highest-priority** *upcoming* alert other than the top alert (ties → earliest onset), if any: `即将：{中文名} {周X HH:mm} 起`. Priority, not onset, so that a coming High Surf Warning is never hidden behind an earlier, minor Small Craft Advisory (✅ exactly the case on 2026-11-12).
- **Title:**
  - if the top alert is in effect: `官方警示生效中 · {中文名}`
  - if only upcoming alerts exist: `官方警示已发布 · {中文名} · {周X HH:mm} 起生效`
- **Subtitle** = `{first extracted fact of the top alert (§7.2 layer 3)}。{short template (§7.1.6)}`, or just the short template if there is no fact. One line, ellipsized.
- **Until** = `至 {周X HH:mm}` (the top alert's `ends ?? expires`).

**7.1.5 Day tags:** a 5-day card gets the `警示` tag if any filtered alert's interval `[onset ?? effective, ends ?? expires)` overlaps that local calendar day.

**7.1.6 Priority and names** (the official event names are **never** machine-translated):

| Prio | NWS `event` | 中文名 | Short template (banner) | Full template (detail 说明) |
|---|---|---|---|---|
| 100 | Tsunami Warning | 海啸警报 | 立即远离海岸前往高处。 | 海啸警报！立即远离海岸前往高处。 |
| 95 | Tsunami Advisory | 海啸提示 | 远离海滩和港口。 | 可能有强海流和危险海浪，远离海滩和港口。 |
| 90 | Tsunami Watch | 海啸预警 | 留意后续通知。 | 可能发生海啸，留意后续通知。 |
| 85 | Storm Warning | 风暴警报 | 海上风暴级大风，切勿出海。 | 海上风暴级大风（48 节以上），切勿出海。 |
| 84 | Special Marine Warning | 特别海事警报 | 船只立即回港避险。 | 短时强雷暴或水龙卷，船只立即回港避险。 |
| 82 | High Surf Warning | 巨浪警报 | 危险巨浪，切勿下水，远离礁石和海边。 | 危险巨浪，海浪可冲上海滩和防波堤。切勿下水，远离礁石和海边。 |
| 80 | Hazardous Seas Warning | 危险海况警报 | 海况危险，船只避免出海。 | 海况危险，浪高且陡，船只避免出海。 |
| 78 | Gale Warning | 大风警报（海上） | 海上大风，小船切勿出海。 | 海上大风（34–47 节），小船切勿出海。 |
| 76 | Coastal Flood Warning | 沿海洪水警报 | 沿岸低洼地区将被淹，避开海边道路。 | 沿岸低洼地区将被海水淹没。避开海边道路、停车场和码头。 |
| 74 | High Wind Warning | 强风警报 | 强风，避免海边活动。 | 强风，避免海边活动。 |
| 70 | **Beach Hazards Statement** | **海滩危险声明** | 可能出现疯狗浪和强离岸流，勿背对大海。 | 可能出现疯狗浪和强离岸流。勿背对大海，远离礁石、防波堤和码头边缘。 |
| 68 | High Surf Advisory | 大浪提示 | 大浪拍岸，远离礁石和防波堤。 | 大浪拍岸，海浪可能突然冲高。远离礁石和防波堤，勿背对大海。 |
| 66 | Rip Current Statement | 离岸流声明 | 强离岸流，勿下水。 | 有强离岸流，可把人迅速卷离岸边。勿下水。 |
| 64 | Small Craft Advisory | 小型船只提示 | 风浪对小船危险，不建议小船出海。 | 风浪对小型船只危险，不建议小船出海。 |
| 62 | Coastal Flood Advisory | 沿海洪水提示 | 涨潮时低洼处可能进水。 | 涨潮时沿岸低洼地区可能进水。注意高潮时段。 |
| 60 | Wind Advisory | 大风提示 | 大风，注意安全。 | 大风，注意安全。 |
| 58 | Dense Fog Advisory | 浓雾提示 | 浓雾，能见度很低。 | 浓雾，能见度很低，航行和驾车请小心。 |
| 50 | Coastal Flood Statement | 沿海洪水声明 | 高潮时沿岸可能小范围积水。 | 高潮时沿岸可能有小范围积水。 |
| 40 | *(any other event)* | *(English event name)* | 请查看英文原文。 | 请查看英文原文。 |

⚠ All Chinese strings in §7 must be reviewed by the maintainer before M4.

### 7.2 Alert text handling (the unstructured parts)

**Safety rule:** text parsing only *adds detail*. Whether an alert exists, its type, its timing and its area always come from structured fields (§7.1). A parsing failure can never hide or delay an alert; the worst case is template plus English.

Reference prototype: `docs/alert_text.py`. It was run on the live 2026-11-11 storm fixture; the results below are from that run.

| Layer | What | How | Result on `fixtures/nws_alerts_point_sf_4alerts_2026-11-11.json` |
|---|---|---|---|
| 0. Structured fields | Type, timing, area, action | `event`, `onset`/`effective`, `ends`/`expires`, `affectedZones` (→ Chinese, §7.3), VTEC action | All 4 events correct. WHEN/WHERE prose is never parsed. VTEC `NEW` → `新发布` tag in the detail screen. |
| 1. Section split | `* WHAT...`, `* WHERE...`, `* WHEN...`, `* IMPACTS...`, `* ADDITIONAL DETAILS...` | **Unwrap hard line breaks first** (NWS breaks mid-phrase, e.g. `rip\ncurrents`), then split on `\* ([A-Z /]+)\.\.\.` | 5 sections |
| 2. Per-event clause | One WHAT covering several events | Split on `For the (first\|second\|third )?<Event>,`. The *n*-th clause for an event goes to the *n*-th feature of that event, sorted by `onset`. No `For the` → the whole section belongs to the single event. | BHS, HSW, CFA #1 and CFA #2 each got the right clause |
| 3. Facts | Numbers that matter for fishing | Pattern → Chinese template; ft → m (1 decimal) with ft kept: `breaking waves of X to Y ft` → `破浪 a–b米（X–Y英尺）`; `occasional sets of X to Y ft` → `偶有 a–b米（X–Y英尺）的大浪组`; `up to X ft of inundation` → `低洼处可能积水深达 a米（X英尺）`; `X ft above normal (Y ft MLLW) at H:MM AM/PM Day M/D` → `M/D HH:MM 高潮约 a米（比正常高 b米）`; `seas X to Y ft` → `海浪 a–b米（X–Y英尺）`; `winds X to Y kt` → `风速 X–Y 节`. Unmatched → nothing (never guess). | `破浪 4.6–6.1米（15–20英尺）`; `破浪 7.6–10.7米（25–35英尺）`, `偶有 12.2–15.2米（40–50英尺）的大浪组`; `低洼处可能积水深达 0.5米（1.5英尺）`; `11/12 12:10 高潮约 2.2米（比正常高 0.4米）` |
| 4. Keyword chips | Key ideas in IMPACTS / INSTRUCTION | Case-insensitive phrase list on unwrapped text: sneaker wave → 疯狗浪; rip current → 离岸流; over seawalls → 浪越过防波堤; road closure → 道路封闭; jetties → 勿上防波堤; turn your back on the ocean → 勿背对大海; remain out of the water → 勿下水 | `疯狗浪 · 离岸流 · 浪越过防波堤 · 道路封闭 · 勿上防波堤 · 勿背对大海 · 勿下水` |
| 5. Remaining prose | IMPACTS / ADDITIONAL DETAILS / INSTRUCTION | Not translated. Covered by the full template plus the English original. | — |

- Facts from ADDITIONAL DETAILS (e.g. tide-gauge forecasts) belong to the product as a whole. They are shown once under `补充` in the detail screen, not per alert.
- Features sharing an identical `description` show the English original once.
- The port uses `std::regex` or hand-written matchers; choose based on flash/stack cost in M1.
- **Coverage work (M1):** download a corpus with `https://api.weather.gov/alerts?area=CA&event=<Event>&limit=500` for each mapped event. Report the % of alerts with a layer-2 clause and with ≥ 1 layer-3 fact. Add a pattern only when a phrase recurs. In debug mode, log any WHAT clause that yields no fact.

### 7.3 Chinese lookup tables
- **Zones → area text** (✅ names checked against `/zones/forecast/{id}`): CAZ006 San Francisco → 旧金山; CAZ505 Coastal North Bay incl. Point Reyes → 北湾沿海（含雷耶斯角）; CAZ506 North Bay Interior Valleys → 北湾内陆谷地; CAZ508 San Francisco Bay Shoreline → 旧金山湾沿岸; CAZ509 San Francisco Peninsula Coast → 旧金山半岛沿海; CAZ529 Northern Monterey Bay → 蒙特雷湾北部; CAZ530 Southern Monterey Bay and Big Sur Coast → 蒙特雷湾南部及大苏尔沿海; PZZ545 → 雷耶斯角至皮金角沿海水域（10海里内）; PZZ531 → 旧金山湾（海湾大桥以南）; PZZ535 → 蒙特雷湾; PZZ560 → 皮金角至皮诺斯角沿海水域（10海里内）. Any unknown zone shows `areaDesc` in English.
- **Detail section labels:** WHAT → 情况, IMPACTS → 影响, ADDITIONAL DETAILS → 补充, PRECAUTIONARY/PREPAREDNESS ACTIONS → 建议. WHERE/WHEN are not shown in Chinese, because the area and times come from structured fields.
- **Weather codes (WMO, Open-Meteo):**

| code | 中文 | icon |
|---|---|---|
| 0 | 晴 | sun |
| 1, 2 | 晴间多云 | partly |
| 3 | 阴 | cloud |
| 45, 48 | 雾 | fog |
| 51, 53, 55, 56, 57 | 毛毛雨 | rain |
| 61, 66, 80 | 小雨 | rain |
| 63, 81 | 中雨 | rain |
| 65, 67, 82 | 大雨 | rain |
| 71–77, 85, 86 | 雪 | cloud |
| 95, 96, 99 | 雷雨 | storm |
| other | 多云 | cloud |

  📎 `relay/weather.py:7` has the full English code list for cross-checking.
- **Wind direction, 8 sectors** (degrees; the "from" direction): 北 [337.5, 22.5) · 东北 [22.5, 67.5) · 东 [67.5, 112.5) · 东南 [112.5, 157.5) · 南 [157.5, 202.5) · 西南 [202.5, 247.5) · 西 [247.5, 292.5) · 西北 [292.5, 337.5).
- **Weekdays:** 周一…周日; the current date shows as `今天`.

### 7.4 Formatting rules (units per D7)

| Item | Rule | Example |
|---|---|---|
| Wind tile value | current (S3) direction + rounded speed | `西南 3` 节 |
| Wind tile sub | Today's NWS marine wind: `今日海上 阵风 {gust}节` if gusts are stated, else `今日海上 {dir} {lo}–{hi}节`. Fallback: `阵风 {current gust} · 今日最大 {max gust}` | `今日海上 南 15–20节` |
| Wind tile label | `风 · {current.time HH:mm} 实况` | `风 · 05:00 实况` |
| Wave tile (coastal) | Buoy (S6) if the observation is < 3 h old: `Seas` if present, else `Swell`; period from `Peak Period`, else the summary `Period`; metres 1 dp. Otherwise S4 `current.wave_height` / `wave_period`. | `2.0` 米 |
| Wave tile sub / label | `约{ft}英尺 · 周期{s}秒`; label `外海浪高 · 浮标` or `外海浪高 · 预报` (the buoy is only used when its reading is < 3 h old; a longer label did not fit the tile) | `约6.6英尺 · 周期12秒` |
| Bay conditions tile | Label `湾内水况`; value from today's max sustained wind (S3 daily): < 8 平静, 8–14 小浪, 15–20 中浪, > 20 浪大; sub `按风力估计` | `小浪` |
| Weather tile | value `{round min}–{round max}°C`; sub `{today 中文天气} · 实况 {round current temp}°C` | `13–20°C` / `雾 · 实况 13°C` |
| Water tile | coastal: buoy `Water Temp` °F→°C, sub `浮标 {id}`; if missing → S4 `sea_surface_temperature`, sub `海温模型估计`. bay: S7, sub `阿拉米达站`; if missing → S4 SST at the bay point, sub `海温模型估计` | `19°` |
| 5-day temperature | `{min}–{max}°C`; on the card the temperature steps down 28→26 px, then the weather word 20→18 px, when both don't fit | `13–18°C` |
| 5-day wind | **NWS marine (S8)**, from the daytime period of that date (the night period if no daytime period remains): `{dir} {lo}–{hi}节` from the strongest sustained phrase, plus ` 阵{gust}` when gusts are stated. Fallback (no NWS period): Open-Meteo `{dir} {max sustained}节` + ` 阵{max gust}` when higher. The numbers before 节 are always sustained wind. | `南 25–30节 阵40`, `西南 5节` |
| 5-day waves (coastal) | **NWS marine seas**: the highest `Seas … ft` of the period → m (1 dp) · period of the largest `Wave Detail` component. Fallback: Open-Meteo `{wave_height_max}米 · {wave_period_max}秒` | `4.3米 · 14秒` |
| 5-day tidal range (bay) | that date's max(high) − min(low), 1 dp | `2.4米` |
| 5-day tide | that date's highest high and lowest low: `满HH:MM 干HH:MM` | `满11:10 干18:50` |
| Heights | metres, 2 dp on the chart, 1 dp elsewhere; negatives use `−` (U+2212) | `−0.08米` |
| Times | 24 h, America/Los_Angeles | `18:50` |
| Date header | `{M}月{D}日 {周X} {HH:mm}` | `11月12日 周四 06:40` |

### 7.5 Tide model (for the chart and the cards)
- **Input:** the S5 hi/lo predictions (yesterday … today+5), converted to local minutes from today's 00:00.
- **Curve:** between consecutive extremes `(t₀, h₀)` and `(t₁, h₁)`: `h(t) = h₀ + (h₁ − h₀) · (1 − cos(π · (t − t₀)/(t₁ − t₀))) / 2`. Sample every 10 min from 00:00 to 24:00. Reference implementation: `docs/tide_chart.py`. A preview of all three locations is in the prototype chart preview.
- **Today's extremes** = predictions with 00:00 ≤ t < 24:00; usually 3–4.
- **Tidal range today** = max − min of today's extremes, 2 dp. Shown as `今日潮差 1.77米`.
- **DST days:** the day has 23 or 25 h. The x axis spans the real day length in minutes; the hour labels are placed by real local time.
- **Offline:** predictions don't go stale. The cached S5 data covers up to 5 more days without a network. If the cache does not cover today, the chart shows `潮汐数据不足` instead of a curve.

### 7.6 Staleness and failure

| Condition | Effect |
|---|---|
| Any alert request for a location failed in the latest cycle | That location → UNKNOWN (§7.1.3) |
| Last successful alert check > 45 min ago, or the clock is invalid | All locations → UNKNOWN |
| Forecast data (S3/S4) older than 14 h | Footer text turns orange: `预报数据已过期（上次 HH:mm）` |
| One section failed in a full refresh | That section keeps its last good data. Its tile shows the time of that data (D8). |
| Buoy observation ≥ 3 h old or missing | Wave/water tiles fall back as in §7.4 |
| No cache and first fetch failed | S3 splash (§9.6) |

Status area (header right): `已连接` (grey) · `正在更新…` (grey) · `更新失败 · 显示缓存` (#F0937A) · `未连接 Wi-Fi` (#F0937A).

### 7.7 Rebuilding the Report as time passes
The Report depends on `now`: in-effect vs upcoming, ended alerts, `今天`, the tide day, staleness. It is therefore **rebuilt from the cached raw responses** (§10.6), not only after fetches. Rebuild:
- after every fetch cycle (successful or not)
- at local midnight
- at every alert boundary: the next `onset`/`effective` or `ends`/`expires` among the cached alerts
- when the clock becomes valid
- when the 45-min alert staleness limit is crossed

Rules:
- **Day rollover without new data:**
  - The 5-day cards start at local today, taken from the 6-day S3/S4 data.
  - If today is missing from that data, the forecast is stale and the cards show `—`. This can't happen with ≤ 14 h-old data.
  - The tide chart moves to the new day using the cached S5 data.
- A rebuild never fetches anything; it only re-runs `core` on cached inputs with the new `now`.

---

## 8. Data Model & Cache

`core` produces a `Report`: an **in-memory** model with display-ready Chinese strings plus the raw values the UI and tests need. It is **not** the cache. The cache is the raw responses (§10.6), so a Report can always be rebuilt for a new `now` (§7.7). The serial `report` command dumps the current Report as JSON in the format below, for debugging and for test expectations.

Example, built from real data for `halfmoon` with `now = 2026-11-12T06:40-08:00` (abbreviated):
```json
{
  "schema": 3,
  "builtAt": "2026-11-12T05:12:03-08:00",
  "forecastUpdatedAt": "2026-11-12T05:12:03-08:00",
  "nextFullRefresh": "2026-11-12T12:00:00-08:00",
  "places": [{
    "id": "halfmoon", "name": "半月湾", "profile": "coastal",
    "sourcesText": "潮汐 NOAA 9414131 · 浮标 46012 · 警示 NWS",
    "alerts": {
      "state": "ISSUED", "checkedAt": "2026-11-12T06:32:10-08:00",
      "banner": { "title": "官方警示生效中 · 海滩危险声明", "countText": "共4条",
                  "sub": "破浪 4.6–6.1米。可能出现疯狗浪和强离岸流，勿背对大海。",
                  "until": "至 周五 18:00", "next": "即将：巨浪警报 周五 18:00 起" },
      "items": [{
        "id": "urn:oid:2.49.0.1.840.0.…", "event": "Beach Hazards Statement", "nameZh": "海滩危险声明",
        "priority": 70, "vtecAction": "CON", "inEffect": true,
        "start": "2026-11-11T22:48:00-08:00", "end": "2026-11-13T18:00:00-08:00",
        "periodText": "周三 22:48 至 周五 18:00", "areaText": "北湾沿海（含雷耶斯角）、旧金山半岛沿海、蒙特雷湾北部",
        "fullTemplate": "可能出现疯狗浪和强离岸流。勿背对大海，远离礁石、防波堤和码头边缘。",
        "facts": ["破浪 4.6–6.1米（15–20英尺）"],
        "chips": ["疯狗浪", "离岸流", "浪越过防波堤", "道路封闭", "勿上防波堤", "勿背对大海", "勿下水"],
        "descKey": "sha1-of-description"
      }],
      "productFacts": [],
      "englishById": { "sha1-of-description": "* WHAT...(unwrapped)\n* IMPACTS..." }
    },
    "tide": {
      "station": "Pillar Point 站",
      "extremes": [[-351,-0.141,"L"],[71,1.215,"H"],[312,0.958,"L"],[670,1.69,"H"],[1130,-0.078,"L"],[1564,1.183,"H"]],
      "dayMinutes": 1440, "rangeM": 1.77, "sunrise": "06:46", "sunset": "17:01"
    },
    "tiles": {
      "wind":  { "label": "风 · 05:00 实况", "value": "西南 3", "unit": "节", "sub": "阵风 8 · 今日最大 19" },
      "wave":  { "label": "浪高（外海）· 浮标 05:00", "value": "2.0", "unit": "米", "sub": "约6.6英尺 · 周期12秒" },
      "weather": { "label": "天气 · 气温", "value": "13–20°", "sub": "雾 · 实况 13°" },
      "water": { "label": "水温", "value": "19°", "sub": "浮标 46012" }
    },
    "days": [
      { "date": "2026-11-12", "day": "今天", "md": "11/12", "icon": "fog", "wx": "雾", "temp": "13–20°",
        "wind": "南 10–19节", "row2Label": "浪", "row2": "3.2米 · 13秒", "tide": "满11:10 干18:50", "alert": true }
    ],
    "stale": { "forecast": false, "waves": false, "tides": false, "water": false }
  }]
}
```
- `days` always has 5 entries; `places` has one entry per row of the §4 table (4).
- `icon` ∈ `sun | partly | cloud | fog | rain | storm`.
- Raw-cache validation (§10.6): a cached response that no longer parses is deleted and treated as missing for its section. Other sections are unaffected.
- The JSON dump is < 64 KB, including English originals.

---

## 9. UI Specification (Prototype 2)

Reference: the mockup's top row. All values below are the implementation spec; where the mockup and this section differ, **this section wins**. Known differences:
- The tile labels say `现在` in the mockup and show the data time here (D8).
- The mockup's alert detail screen is from Prototype 1 (§9.5).
- The mockup's Redwood City banner shows the Small Craft Advisory as the top alert. Under §7.1.4 the top alert is the in-effect Coastal Flood Advisory, and the Small Craft Advisory is the next chip.

### 9.1 Global
- 1280 × 720 landscape (`setRotation` on the native 720×1280 panel), dark theme, Simplified Chinese.
- Font: Noto Sans SC (smooth VLW subsets, §10.2). Digits use tabular figures.
- Touch targets ≥ 44 px tall.
- Text sizes:
  - ≥ 16 px for chart axis labels
  - ≥ 17 px for the footer
  - ≥ 18 px for labels
  - 21 px body in the 5-day cards
  - 38 px tile values
- ⚠ G3 risk: at ~294 PPI these sizes are small. The M2 readability check with the user is a gate. If it fails, the fallback is to move the 5-day cards to a second page (swipe), not to shrink text further.

### 9.2 Design tokens

| Token | Value | Use |
|---|---|---|
| bg | `#0B1620` | screen |
| panel / panelBorder | `#142433` / `#22384C` | cards, tiles |
| text / textSub / textMuted | `#EEF3F7` / `#C9D6E2` / `#9FB2C3` | primary / secondary / labels |
| accent | `#7FD3F2` | icons, tide curve, high-tide labels |
| sun | `#F5C451` | sun icon, sunrise/sunset text, low-tide labels |
| alert / alertText | `#F5A524` / `#1A1206` | ISSUED banner, day tags, alert-day card border (2 px) |
| nextChip | `#FFE2B0` bg, `#1A1206` text | upcoming-alert chip |
| none | `#16324A` bg, `#5EC3E8` 2 px border | NONE banner |
| unknown | `#2B2226` bg, `#F0937A` 2 px dashed border | UNKNOWN banner, error status |
| chartNight | `#07111A` | before sunrise / after sunset |
| chartGrid / chartAxis | `#2A4258` / `#3A5670` | grid lines / baseline |
| chartFill | `#7FD3F2` at 16 % alpha | area under the curve |

### 9.3 S1 — Main screen (per location)
Padding 14 px vertical / 20 px horizontal; 10 px gaps. Top to bottom:

1. **Header (48 px):** (English mode: tabs drop to 21 px when the English names don't fit; a `EN`/`中文` language pill follows the tabs, D11)
   - left: one pill tab per location, `半月湾 | 旧金山 | 红木城 | 圣克鲁斯` (24 px bold, 46 px tall; active = filled `text` color)
   - right: date + time (`11月12日 周四 06:40`, 21 px bold), Wi-Fi icon and status text (§7.6)
   - long-press the right area → manual full refresh
2. **Alert banner (84 px, full width).** Content from §7.1.4. Three variants:
   - **ISSUED** (alert colors): warning icon 44 px; title 26 px bold + `共N条` outline chip; subtitle 20 px, one line, ellipsized; right side: `至 …` 20 px bold + dark `全文 ›` button, and the `即将…` chip under them if present. The whole banner is tappable → S2.
   - **NONE:** info icon; `截至 HH:mm，未查询到适用的官方警示` (HH:mm = last successful check) + `未发布警示不等于安全。出海前请再次留意现场海况。`
   - **UNKNOWN:** dashed border; `警示状态未知：更新失败` + `无法确认是否有官方警示。出海前请查看 weather.gov 或 NOAA 天气广播。`. Tappable → S2 with the cached list, if any.
3. **Middle row (254 px):**
   - **Tide chart card**, 764 px wide (§9.4).
   - **2×2 condition tiles**, filling the remaining width: wind · waves (or bay conditions) · weather/temperature · water temperature. Each tile has: icon + label 18 px, value 38 px bold + unit 20 px, sub 18 px (§7.4).
4. **5-day cards (fill the rest, ≈ 232 px; 5 equal columns).** Each card has:
   - row 1: `今天`/`周X` 25 px bold, `M/D` 18 px, `警示` tag (16 px) if tagged
   - row 2: weather icon 34 px + temperature 28 px bold + weather text 20 px, right-aligned
   - rows 3–5: `风 …`, `浪 …` / `潮差 …`, `潮 满… 干…` (21 px, label in textMuted)
   - Tagged days get the 2 px alert-color border.
5. **Footer (22 px, 17 px text):**
   - left: `预报更新 HH:mm · 警示检查 HH:mm（每15分钟）· 下次完整更新 HH:mm`
   - right: `sourcesText`

### 9.4 Tide chart
- **Card:** 764 × 254, padding 6/14/4 px.
- **Card header (one line):** left: icon + `今日潮汐 · {station}` (20 px, textMuted); right: `今日潮差 {range}米` (21 px bold).
- **Drawing area:** 732 × 210. Plot area x 40 → 724, y 30 → 164.
- **Y scale:** min −0.5 m; max `ceil(2 · maxHeight)/2 + 0.2` m. Grid lines at each whole metre inside the range: 0 m solid, others dashed `4 6`. Labels `0米`, `1米`, … at x = 34, right-aligned, 16 px.
- **X axis:** baseline at y = 164. Labels `0点 6点 12点 18点 24点` at y = 204, 16 px. `24点` is right-aligned to stay inside the area.
- **Night:** chartNight rectangles from 00:00 to sunrise and from sunset to 24:00. `日出 HH:mm` (left-aligned at sunrise + 6 px) and `日落 HH:mm` (right-aligned at sunset − 6 px) at y = 158, 16 px, sun color.
- **Curve:** the samples from §7.5 as a polyline, 3.5 px accent stroke, round joins; area below it to the baseline filled with chartFill.
- **Extreme labels**, for each of today's extremes:
  - two centered lines: `满 HH:MM` / `干 HH:MM` (18 px bold; accent for highs, sun for lows) and `{h}米` (16 px textSub)
  - high: first-line baseline at y − 26; low: at y − 34 (above the trough, so it never collides with the axis)
  - x clamped to [44, 688]
- **No "now" marker** (D3). The chart is drawn once per data change into its own sprite and reused for every frame.

### 9.5 S2 — Alert detail
The mockup's detail screen is from Prototype 1 and is **out of date**. This spec wins. Layout:
1. **Header (48 px):** `‹ 返回 {location}` button (left); `来源：美国国家气象局（NWS）` (right).
2. **Alert list (left column, 320 px; rows 74 px: name 26 px bold, status 21 px), shown when N ≥ 2:** one row per alert. In-effect alerts first by priority, then upcoming by onset. Each row: color bar, `中文名`, `生效中` or `{周X HH:mm} 起`. Tap to select; the top alert is pre-selected.
3. **Selected alert (right):**
   - **Type sizes (enlarged 2026-11-12 after feedback that the Chinese was too small):** title `中文名` 38 px bold + English `event` 21 px; 时间/区域/说明/要点 labels 26 px bold; time and area 26 px; 说明 and 要点 text **28 px**; chips 24 px; 补充 24 px; English original 20 px; back button 24 px. Line breaking never starts a line with closing punctuation (。，、）…).
   - title `中文名` + English `event` + `新发布` tag if VTEC NEW
   - `时间 {periodText}` · `区域 {areaText}`
   - `说明` = full template
   - `要点` = facts as bullets (hidden if none)
   - keyword chips
   - a collapsible `英文原文` panel (default collapsed; shows the unwrapped English sections; scrollable)
4. **`补充`:** product-level facts (e.g. tide-gauge forecasts), once.
5. **Footer:** `中文为摘要说明，以英文原文为准 · 原文见 weather.gov`.
- In the UNKNOWN state, a banner at the top says `以下为 HH:mm 的缓存信息，当前状态未知`.
- Returns to S1 after 3 min idle.

### 9.6 S3 — Splash / no data
Shown only when there is no valid cache and the first fetch failed: `暂无数据 · 请检查 Wi-Fi` + a retry countdown. The boot splash `钓鱼海况 · 正在连接…` is shown for at most a few seconds before the cache or S3 appears.

### 9.7 Rendering and redraw policy
- Full-screen `M5Canvas` in PSRAM (1280 × 720 × 16 bpp ≈ 1.8 MB) → `pushSprite`. The tide chart is a separate cached sprite.
- **Redraw the full screen only** on: a data change, a tab change, an alert-state change, or entering/leaving S2.
- **Partial redraw of the header only** once a minute (clock).
- As built, the tide chart is drawn straight into the full-screen canvas instead of a separate cached sprite. Full redraws are rare, so the extra sprite wasn't needed.
- **Report rebuilds** (§7.7) trigger a full redraw only if the rebuilt Report differs from the one on screen.
- Icons: drawn with primitives (lines/arcs) to match the mockup; no emoji, no bitmaps required.
- The selected tab is persisted in NVS and restored at boot.

---

## 10. Device Platform

### 10.1 Hardware facts (✅ probed over USB, 2026-11-11)

| Item | Value | Implication |
|---|---|---|
| USB | `/dev/cu.usbmodem101`, Espressif USB-Serial/JTAG (303a:1001) | No driver needed. esptool enters download mode automatically (no buttons). Opening the port with some tools resets the board (`rst:0x17 CHIP_USB_UART_RESET`), which is not a crash. |
| SoC | ESP32-P4 rev v1.3, dual core + LP core | Toolchain must support P4 rev ≥ v1.0 |
| MAC | (device-specific, not published) | Reserve a DHCP address for OTA |
| Flash / PSRAM | 16 MB QIO / 32 MB (≈ 27 MB on the heap) | Memory is not a constraint |
| Wi-Fi | ESP-Hosted over SDIO to the ESP32-C6, 2.4 GHz | §10.5 |
| Display / touch | ST7121 / ST7123 (the newer variant), native 720 × 1280 MIPI-DSI; 10-point touch | Needs current M5GFX; rotate to landscape |
| RTC | RX8130 with backup battery (currently unset) | §10.4 |
| Power | INA226 monitor; 7.72 V bus → a 2S battery is installed; charging + QC on | Runs through power cuts; report battery/USB in `status` |
| Other | BMI270 IMU, ES8388 + ES7210 audio, camera, RS485, USB host | Unused in v1 (speaker chime is a v2 idea) |
| Factory firmware | Tab5 UserDemo v2.0.0 (2026-03-25), ESP-IDF v5.4.2 | |
| **Factory backup** | Make one before flashing: `esptool --chip esp32p4 -p <port> -b 921600 read-flash 0 0x1000000 tab5-factory-16MB.bin` (kept locally, not in this repo) | Restore: `esptool --chip esp32p4 -p /dev/cu.usbmodem101 -b 921600 write-flash 0 tab5-factory-16MB.bin` |

Tooling: PlatformIO is **not installed** on this Mac yet (`brew install platformio`).

### 10.2 Toolchain and libraries
- Start from 📎 `emulator/platformio.ini:233` (`[env:board_Tab5]`):
  ```ini
  [env:board_Tab5]
  platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip  ; pin a release in M0
  framework = arduino
  board = esp32-p4-evboard
  extra_scripts = pre:support/risc_arm_cleanup.py        ; copy from the earlier project's emulator/support/
  board_build.partitions = partitions.csv                 ; §10.6
  build_flags = -D M5GFX_BOARD=board_M5Tab5 -D BOARD_HAS_PSRAM -D ARDUINO_USB_CDC_ON_BOOT=1 -D ARDUINO_USB_MODE=1
  lib_deps = m5stack/M5Unified, m5stack/M5GFX, bblanchon/ArduinoJson@^7
  ```
- `[env:emulator_Tab5]` (📎 `emulator/platformio.ini:219`, plus `emulator/support/sdl2_build_extra.py` and `emulator/src/utility/sdl_main.cpp`; needs `brew install sdl2`) runs `ui.cpp` on the Mac.
- `[env:native]` runs the `lib/core` unit tests (`pio test -e native`).
- **Fonts:** smooth VLW fonts made from Noto Sans SC, subset to the characters in the string tables + digits/punctuation, at 16/17/18/20/21/24/25/26/28/32/38 px, stored in LittleFS and loaded with M5GFX `loadFont`. M0 may use the built-in `fonts::efontCN_24` as a temporary stand-in. ⚠ VERIFY the VLW tooling in M2.
- **JSON:** ArduinoJson v7 with a **filter document** for NWS responses (keep only the fields in §7.1/§7.2); documents allocated in PSRAM.
- **HTTP:** `HTTPClient` + `WiFiClientSecure` with the built-in CA bundle; never `setInsecure()`. ⚠ VERIFY that all 5 hosts validate (M3).
- Reference: M5Stack `M5Tab5-UserDemo` (hardware init), docs.m5stack.com → Tab5.

### 10.3 Boot sequence
1. Init display, rotation and backlight. Show the boot splash.
2. Read the RTC. If it is valid, set the system time (the TZ string is applied before any formatting).
3. Load the raw-response cache (§10.6), build the Report with the current `now` (§7.7) and draw S1 immediately, applying the §7.6 staleness rules. If no section can be built, show S3.
4. Start Wi-Fi (§10.5). On connect, sync NTP (`pool.ntp.org`) and write the result to the RTC.
5. Run a full refresh, then follow the §6.3 schedule.

### 10.4 Time
- TZ `PST8PDT,M3.2.0,M11.1.0`. All schedule times are local. DST is handled by the TZ rules.
- The clock is **valid** if NTP succeeded since boot, or the RTC reads a year ≥ 2025. If the clock is invalid, alerts are UNKNOWN (§7.6).
- Re-sync NTP every 6 h and write the time to the RTC.
- `core` never reads the clock; `now` is passed in.

### 10.5 Wi-Fi (lessons from 📎 `firmware/src/wifi_service.cpp:40`, `:103`; playbook §11.5–11.7)
Write a small connect routine; don't copy the earlier project's whole `WifiService`.
1. Before `WiFi.begin`: `WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN); WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);` so a mesh network joins its strongest node.
2. Request HT20 bandwidth **before** associating. ⚠ VERIFY that this and the power-save call are honored through esp-hosted (M0).
3. `WiFi.persistent(false)`, `WiFi.setAutoReconnect(true)`, power save off after connect.
4. 2.4 GHz only. If fetches are flaky, check the router's 2.4 GHz channel first (fixed 1/6/11, 20 MHz).
5. Connect timeout 20 s, then keep retrying in the background. A link loss shows `未连接 Wi-Fi` within one cycle; it must never look like a freeze.

### 10.6 Storage
- **Partitions (16 MB):** nvs 24 KB, otadata 8 KB, phy_init 4 KB, app0 **4 MB**, app1 **4 MB** (OTA), LittleFS ≈ 7.9 MB (fonts 3.5 MB + cache). ✅ The app is 3.03 MB; it filled 96 % of the originally planned 3 MB slot, so the slots were enlarged.
- **NVS:** selected tab, Wi-Fi credentials seeded from `secrets.h`, last good check times.
- **LittleFS:**
  - `/cache/{source}_{key}.txt` (key = place, zone or station, e.g. `S2_PZZ545`): the last good raw response per request (S1…S7), written atomically (`.tmp` + rename)
  - `/cache/meta.json`: per request, the time of the last attempt, the time of the last success and the last error
  - `/fonts/*.vlw`
  - Total cache ≈ 100 KB.

### 10.7 Power & display
- Brightness 80 % from 05:00 to 21:30, 15 % from 21:30 to 05:00. A touch raises it to 80 % for 2 min.
- Optional config: screen fully off 23:00–04:30 (§15 Q5).
- Battery and USB state are reported in `status` (INA226).

### 10.8 Diagnostics (USB serial, one JSON command per line → one JSON reply)
- `status` → `{fw_version, uptime_s, reset_reason, heap_free, psram_free, wifi{ssid,rssi,ip,status}, clock_valid, last_alert_check{at,ok_per_place}, last_full_refresh{at,ok_per_source}, last_error, power{vbus,battery}}`
- `fetch` (run a full refresh now) · `alerts` (alert cycle now) · `report` (dump the current Report, §8) · `debug on|off` (verbose logging, off at every boot) · `reboot`
- Added during implementation: `wifi_set` (save Wi-Fi credentials), `shot` (screen as PNG, used to check the rendering), `tap x y [hold]` (simulated touch), `settime` (set clock + RTC; testing).
- ✅ Opening the port must **not** drop DTR while RTS is asserted, or the P4 resets. `tools/tab5` leaves both lines at their defaults.
- `Serial.setRxBufferSize(1024)` before `Serial.begin`. All verbose output goes behind `debug`, because a full TX buffer blocks prints when no host is reading.
- Record `esp_reset_reason()` at boot (📎 `firmware/src/main.cpp:35–40`).
- Host helper: a tiny `tools/serial.py` (📎 playbook §18.4).

### 10.9 Updates
- ArduinoOTA on the LAN with a password: `pio run -e board_Tab5 -t upload --upload-port <ip>`. The IP is reserved by MAC.
- `FW_VERSION` is bumped every build and reported in `status`.
- USB flashing and the factory restore (§10.1) remain the fallbacks.

### 10.10 Configuration
- `include/secrets.h` (gitignored; template `secrets.example.h`; pattern from 📎 `firmware/src/wifi_secrets.example.h`): `WIFI_SSID`, `WIFI_PASS`, `OTA_PASSWORD`, `UA_CONTACT`.
- `src/config.h`:
  - locations table (§4)
  - schedule times
  - `ALERT_STALE_MIN = 45`, `FORECAST_STALE_H = 14`, `BUOY_FRESH_H = 3`
  - `UNITS = METRIC`
  - brightness schedule

---

## 11. Safety & Content Rules (MUST)

1. Only NWS (`api.weather.gov`) can produce the ISSUED state. No other source affects the alert banner.
2. The absence of alerts is phrased only as `截至 HH:mm，未查询到适用的官方警示`, together with `未发布警示不等于安全…`. Never use 安全 / 无危险 / 适合出海 or similar.
3. Failure is explicit (UNKNOWN), never shown as NONE.
4. Unmapped alert events are shown (priority 40, English name), never filtered out.
5. Upcoming alerts are shown (title or next chip, day tags) as soon as NWS issues them.
6. Chinese alert text is labeled `中文为摘要说明，以英文原文为准`. The English original is always one tap away.
7. Text parsing can only add detail. It never decides whether an alert exists or when it applies (§7.2).
8. Bay locations never show offshore wave heights.
9. Every screen shows its data sources (footer) and data times (tile labels, footer).

---

## 12. Milestones & Acceptance Criteria

Time estimates are for one developer working part-time. Every milestone ends with `pio test -e native` passing.

### M0 — Bring-up (0.5–1 day)
- Install PlatformIO; create the repo (§5.1); set up `board_Tab5`, `emulator_Tab5` and `native` envs; pin platform/library versions that work.
- ✅ "钓鱼海况 你好" renders on the device (landscape) **and** in the emulator.
- ✅ Touch works. Wi-Fi connects with the §10.5 rules. NTP time is correct in PT and written to the RTC; after a reboot without Wi-Fi the time is still correct.
- ✅ `status` works over serial.
- ✅ Done already: factory backup.

### M1 — Core logic on the Mac (2–3 days)
- `lib/core`: all parsers, alert pipeline (§7.1), alert text layers (§7.2), tide model (§7.5), Chinese formatting (§7.3–7.4), rebuild rules (§7.7), Report JSON dump (§8).
- ✅ With `fixtures/nws_alerts_point_sf_2026-11-12.json` + `fixtures/nws_alerts_zone_PZZ545_2026-11-12.json` and `now = 2026-11-12T06:40-08:00`, `sf` gives:
  - state ISSUED, `共5条`
  - top = 海滩危险声明 (in effect; the higher-priority 巨浪警报 is upcoming)
  - next chip `即将：巨浪警报 周五 18:00 起` (not the Small Craft Advisory that starts earlier, Fri 03:00, §7.1.4)
  - until `至 周五 18:00`
  - the subtitle starts with `破浪 4.6–6.1米`
  - day tags Thu–Sun on, Mon off
- ✅ The same fixtures with `now = 2026-11-14T06:00-08:00` give top = 巨浪警报 (now in effect); the BHS and the SCA are filtered out as ended.
- ✅ `redwood` (`nws_alerts_point_rwc_2026-11-12.json` + `nws_alerts_zone_PZZ531_2026-11-12.json`, same `now`): ISSUED, `共2条`, top = 沿海洪水提示 (in effect), next chip `即将：小型船只提示 周四 21:00 起`.
- ✅ The layer-2/3/4 outputs match the table in §7.2 exactly.
- ✅ All NDBC variants parse: `ndbc_latest_obs_46012*.txt` (Seas), `ndbc_latest_obs_46026.txt` (summary only, with wind and water temp), `ndbc_latest_obs_46026_2026-11-12.txt` (Seas + summary, no wind, no water temp → water tile falls back to SST). This includes the 0xB0 byte.
- ✅ Rebuild: the 2026-11-12 fixtures rebuilt at `now = 2026-11-13T01:00-08:00` (no new data) give cards starting `今天 11/13`, the tide chart for 11/13, and the BHS still in effect.
- ✅ The tide model reproduces `docs/tide_chart.py`: the same extremes, today's range 1.77 / 1.83 / 2.45 m for halfmoon / sf / redwood on 2026-11-12, and samples within ±0.01 m.
- ✅ Alert corpus coverage report produced (§7.2).

### M2 — UI from fixtures (2–3 days)
- `ui.cpp` + `ui_chart.cpp` draw S1 (all 3 alert states, coastal and bay), S2 and S3 from fixture-built Reports. Build in the emulator first, then on the device.
- VLW fonts generated and loaded from LittleFS.
- ✅ Side-by-side with the Prototype 2 mockup: same layout, sizes and colors (§9). The chart matches the prototype chart preview.
- ✅ **Gate:** The user reads S1 on the real device at their normal distance. He can say within 3 s whether an official alert is issued, and he can read the tide times. If not, apply the §9.1 fallback and repeat.

### M3 — Live on the device (2 days)
- `net.cpp` (TLS, UA, filter docs), schedule (§6.3), raw cache (§10.6), rebuild timers (§7.7), staleness (§7.6), status texts.
- ✅ All 7 sources fetched over TLS with certificate validation.
- ✅ The screen matches the live sources (spot-check against weather.gov, tidesandcurrents.noaa.gov and ndbc.noaa.gov).
- ✅ Wi-Fi off for 50 min → all banners UNKNOWN; back on → recovers within 15 min.
- ✅ Reboot without Wi-Fi → cached data drawn within 3 s with the correct time (RTC); tides still correct.
- ✅ Overnight without Wi-Fi → at 00:00 the cards and the tide chart move to the new day; the alert banner turns UNKNOWN after 45 min, as specified.

### M4 — Detail screen and content review (1–2 days)
- S2 complete (§9.5): multi-alert list, facts, chips, English panel, `补充`.
- ✅ The maintainer has reviewed and approved all Chinese strings (§7.1.6, §7.3).
- ✅ `nws_alerts_point_sf_2026-11-12.json` + `nws_alerts_zone_PZZ545_2026-11-12.json` at `now = 2026-11-12T06:40-08:00` show all 5 alerts: in effect first (海滩危险声明, 沿海洪水提示), then upcoming by onset (小型船只提示 Fri 03:00, 巨浪警报 Fri 18:00, 沿海洪水提示 Sat 00:00), with `补充` showing the two tide-gauge facts once.

### M5 — Hardening (ongoing, first 2 weeks)
- OTA, brightness schedule, a `README.md` (build / flash / OTA / factory restore), pinned versions.
- ✅ 7 days unattended with no stuck screen, no reboots other than power-on (checked via `status`), and heap stable.
- ✅ Behavior checked across a schedule boundary and at least one real NWS alert issued/expired during the soak.
- ⚠ The DST transition (2027-03-14) is checked by a unit test with a mocked `now`, not in the soak.

---

## 13. Test Plan

| Area | Tests (native unless noted) |
|---|---|
| Alert pipeline | none; 1 alert; in effect + upcoming; upcoming only (title variant); CAN/EXP filtered; `ends` null → `expires`; unmapped event shown; duplicate ids across land + marine; priority ordering; day-tag overlap at midnight |
| Alert text | section split with mid-phrase line breaks; ordinal clauses (first/second); facts per pattern; no-fact case; keyword chips; shared description shown once |
| Tide | cosine curve vs prototype; extremes across midnight; 3-extreme days; DST 23 h / 25 h days; negative heights; cache that doesn't cover today → `潮汐数据不足` |
| Formatting | wind sectors at boundaries (22.5°, 337.5°); gust ≤ sustained; rounding; ft↔m; °F→°C; `−` sign; weekday / `今天` |
| Parsers | all three NDBC variants (§12 M1), `MM`/missing values, 0xB0 byte; CO-OPS error JSON (`"error"`); Open-Meteo missing fields; NWS 33 KB document within the filter budget |
| Staleness | per-location alert failure → UNKNOWN; > 45 min → all UNKNOWN; invalid clock; forecast > 14 h; partial section failures |
| Cache & rebuild | an unparsable cached response is dropped per section; rebuild at midnight, at alert onset/end and at the 45-min staleness limit (mocked `now`); atomic write survives a power cut mid-write (device) |
| Device | TLS failure, timeout, Wi-Fi loss, NTP failure with a valid RTC, OTA, 7-day soak |
| UX | the M2 gate with the user; the 3-second alert-recognition check |

Fixtures: `fixtures/`. They hold a full set for every source captured 2026-11-12 ≈ 00:26 PST (files suffixed `_2026-11-12`, the data behind Prototype 2), plus the 2026-11-11 storm captures. Add the M1 alert corpus there.

---

## 14. Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Text too small for the user on the 5" screen | Medium | M2 gate; second-page fallback (§9.1) |
| Upstream format changes (NWS text, NDBC layout) | Medium | Parsers fail soft per section; alert state comes only from structured fields; OTA fixes |
| esp-hosted Wi-Fi / TLS quirks on P4 + C6 | Medium | Verified in M0/M3 before UI work depends on them |
| A missed alert due to a parsing bug | Low | Safety rules §11 (1–7); unmapped events shown; UNKNOWN on any failure |
| Chinese template wording misleads | Low | the maintainer's review (M4); English always available |
| API rate limits / outages | Low | ≈ 530 requests/day; per-section stale handling; tides cached for days |

---

## 15. Open Questions

1. Units: does the user prefer 米/°C/节, or feet/°F/mph? (Default D7; one config switch.)
2. Exact fishing spots: Pillar Point Harbor jetty vs Half Moon Bay beaches; Pacifica Pier vs Ocean Beach for 旧金山? This affects the forecast points only, not the alert zones.
3. Phone notification for new alerts (v2, Appendix A.4): wanted? Which app does the user use?
4. Should the 5-day forecast move to a second page if the M2 readability gate fails? (Proposed fallback.)
5. Night behavior: dim (default) or fully off?

---

## 16. Verification Checklist

Verified:
- [x] Tide stations 9414131 / 9414290 / 9414523; water temp only at 9414750 (bay) and the buoys (coast)
- [x] Zones CAZ509 / CAZ006 / CAZ508 (and the names of every zone in §7.3); marine PZZ545 / PZZ531; land points required for `/points`
- [x] Alert rules §7.1.3–7.1.5 checked against the 2026-11-12 fixtures (the M1 numbers come from that run)
- [x] NWS accepts a custom User-Agent; response sizes measured (§6.1)
- [x] NWS `alerts/active` includes future-onset alerts (§6.2, §7.1)
- [x] Open-Meteo daily `sunrise`/`sunset` available
- [x] Tab5 hardware facts, display variant, auto download mode; factory backup made

Verified during implementation (2026-11-12):
- [x] pioarduino `espressif32@55.3.312` (Arduino core 3.3.12, ESP-IDF 5.5) + M5Unified 0.3.0 + M5GFX 0.3.0 + ArduinoJson 7.4.3 drive the ST7121/ST7123 display, touch, RTC and esp-hosted Wi-Fi
- [x] Wi-Fi needs `WiFi.setPins(12, 13, 11, 10, 9, 8, 15)` (SDIO2 to the C6) before `WiFi.begin`
- [x] RX8130 RTC read/write via `M5.Rtc`; time survives reboots
- [x] VLW fonts generated by `tools/make_fonts.py` load from LittleFS into PSRAM (455 glyphs × 16 sizes = 3.5 MB)
- [x] TLS with `useBuiltinCACertBundle()` to all 5 hosts; 16/16 fetches OK on the first live run
- [x] ArduinoJson filter for the 33 KB alert documents; heap stable (≈ 290 KB internal free)
- [x] OTA over the LAN (`pio run -e ota`)
- [x] **Accuracy audit** (`tools/audit.py`): the device Report is compared field by field against an independent Python recomputation from the same sources fetched at the same time. 107/107 fields match (2026-11-12 01:35 and 01:50); **143/143** with Santa Cruz (14:58). Cross-source sanity: water 19°C confirmed by 5 buoys and a 10-day trend; air highs within 1°C of the NWS point forecast; waves consistent with the NWS seas ranges. This found the wind under-statement that led to D9.
- [x] `std::regex` over long NWS text overflowed the 8 KB loop stack. Section/clause parsing is now hand-written, and the loop stack is 64 KB.

To verify (milestone):
- [ ] Pin the versions listed above in `platformio.ini` (currently `stable` / latest)
- [ ] HT20 bandwidth request (§10.5 rule 2) **not implemented yet**; Wi-Fi is stable at −54 dBm without it. Add it only if fetches flake.
- [ ] M2 readability gate with the user on the real device
- [ ] 7-day soak (M5)
- [ ] Chinese string review (M4)

---

## 17. Reference Index

### 17.1 Reused from an earlier private project ("mochi", not public)

| Need | Look at | How |
|---|---|---|
| Tab5 build env | `emulator/platformio.ini:233`, `emulator/support/risc_arm_cleanup.py` | Copy (§10.2) |
| Tab5 desktop emulator | `emulator/platformio.ini:219`, `emulator/README.md`, `emulator/support/sdl2_build_extra.py`, `emulator/src/utility/sdl_main.cpp` | Copy (§10.2) |
| Wi-Fi connect rules | `firmware/src/wifi_service.cpp:40`, `:103` | Ideas only (§10.5) |
| Secrets pattern | `firmware/src/wifi_secrets.example.h`, `.gitignore`, `wifi_service.cpp:7` | Same pattern (§10.10) |
| Reset reason | `firmware/src/main.cpp:35–40` | `status` (§10.8) |
| Weather code names | `relay/weather.py:7` | Cross-check (§7.3) |
| Portable core + desktop tests pattern | `engine/` | Same idea, much smaller (§5) |
| Serial harness | `docs/ENGINEERING_PLAYBOOK.md` §18.4, `tools/llm_bridge.py` (`write_all`) | `tools/serial.py` |

Playbook sections to read first: §6.1–6.4 (flashing, serial, reset reasons), §11.5–11.7 (Wi-Fi), §15 (debugging triage), §16 (mistakes → rules), §17 (new-project checklist; skip the audio/LLM items).
Deliberately not reused: the face engine, voice/LLM relay, WebSocket client, custom JSON parser, CoreS3 RAM tuning, HIL test tooling.

### 17.2 Project artifacts (in this repository)

| Path | What |
|---|---|
| `docs/PRD.md` | This document. |
| `fixtures/` | Real API responses for every source (2026-11-11 and 2026-11-12), incl. a live multi-alert storm |
| `docs/alert_text.py` | Reference for §7.2 |
| `docs/tide_chart.py` | Reference for §7.5 / §9.4 |

### 17.3 External

| Topic | Where |
|---|---|
| Tab5 docs / demo | docs.m5stack.com → Tab5; GitHub `m5stack/M5Tab5-UserDemo` |
| M5GFX (VLW `loadFont`, `M5Canvas`) | GitHub `m5stack/M5GFX` |
| ArduinoJson v7 (filters, allocators) | arduinojson.org |
| NWS API | https://www.weather.gov/documentation/services-web-api ; marine zones https://www.weather.gov/mtr/ |
| NOAA CO-OPS API | https://api.tidesandcurrents.noaa.gov/api/prod/ |
| NDBC | https://www.ndbc.noaa.gov/ |
| Open-Meteo | https://open-meteo.com/en/docs , https://open-meteo.com/en/docs/marine-weather-api |

---

## Appendix A — Alternatives Considered / Deferred

**A.1 Google Apps Script as the backend (rejected for v1, D1).** Apps Script would fetch every source on time triggers, machine-translate with `LanguageApp`, store `report.json` in Drive, and serve it from `doGet`; the Tab5 would only render. Rejected because:
- it adds a second system
- the NWS User-Agent can't be verified there
- `doGet` answers with a 302 redirect, which the device would have to follow
- it depends on a Google account
- machine translation garbles NWS's bundled multi-event text

Revisit only if device-side fetching proves unreliable.

**A.2 Home-server fallback.** If device-side fetching ever fails, the same `core` logic (or a Python port) could run on the an existing home server and serve `report.json` on the LAN.

**A.3 LLM translation (v1.1 candidate).**
- The device sends each distinct description once to the Claude Messages API (`claude-haiku-5-5`) and caches the result by description hash in LittleFS.
- The key lives in `secrets.h` (a dedicated key with a low spend limit).
- **Guardrail:** every number in the output must appear in the English source (after unit conversion), otherwise the result is discarded.
- The result is labeled `机器翻译`. If it fails or is discarded, the display falls back to §7.2.

**A.4 Phone notifications (v2 candidate).** On the first sighting of a new alert `id`, POST a short Chinese message to `https://ntfy.sh/<unguessable-topic>`. The user subscribes in the ntfy app. No account and no server needed.

**A.5 Hourly current-conditions refresh (not adopted).** Refreshing wind/buoy/water every hour (+6 requests/hour) would let the tiles say `现在`. Not adopted for v1, to keep the 3×/day requirement; the tiles show their data time instead (D8).

---

## Appendix B — Change Log

| Version | Date | Changes |
|---|---|---|
| v1 | 2026-11-11 | Initial PRD: Apps Script backend, Prototype 1 UI |
| v2 | 2026-11-11 | Hardware probe; APIs verified live; earlier-project references; device-only design proposed; alert-text layers |
| v3.5 | 2026-11-12 | D11: English option for the landing page (English event names, templates, facts, weather words, compass points, H/L tide notation, `S 25–30kt G40` wind); 15 native tests. Audit: Open-Meteo can serve two model runs from different servers right after an update, so model temperatures are compared with ±1°C (days 0–2) and ±2°C (days 3–4) tolerance; seen live: Santa Cruz Monday high 16.1 vs 14.4°C from two servers at the same moment. |
| v3.4 | 2026-11-12 | Temperatures labelled °C everywhere (D7 confirmed). |
| v3.3 | 2026-11-12 | D10: Santa Cruz added (4 locations; fetch plan 7 + 25 requests); alert detail page type enlarged (body 28 px) with CJK line-break rules; 14 native tests. |
| v3.2 | 2026-11-12 | Accuracy audit; D9: wind/gusts/seas from the NWS marine zone forecast (S8); serial log lines moved behind `debug` (they corrupted JSON replies). |
| v3.1 | 2026-11-12 | Implemented; deviations and verifications recorded (Appendix C, §16, §10.6, §10.8, §6.2, §7.4). |
| **v3** | 2026-11-12 | **Baseline for implementation.** <br>• Device-only adopted (D1); Apps Script moved to Appendix A. <br>• UI rebuilt on Prototype 2 (D2): tide chart spec without a now marker (D3), compact banner with count and next chip, 2×2 tiles. <br>• Alert pipeline distinguishes in-effect vs upcoming (NWS returns future-onset alerts); banner rules redefined; states renamed ISSUED / NONE / UNKNOWN. <br>• Tide fetch window extended to yesterday…today+5; tide model specified. <br>• Templates split into short (banner) and full (detail); detail screen respecified (the mockup's S2 is outdated). <br>• Tiles show their data time (D8). <br>• The cache holds raw responses; the Report is rebuilt as time passes (§7.7: midnight, alert boundaries, staleness); 6-day forecast fetch. <br>• Partitions with OTA slots; serial `report` dump. <br>• Next-chip rule = highest-priority upcoming alert; zone names verified; NDBC parsing made key-based. <br>• Acceptance criteria rewritten against real fixtures; risks section added. |

---

## Appendix C — Implementation Notes (v3.1)

| Item | As built |
|---|---|
| Code | this repository (`README.md` has build / flash / OTA / restore steps) |
| Toolchain | PlatformIO in a Python 3.13 venv (`~/.pio-venv`). pioarduino rejects Python 3.15. the earlier project's `risc_arm_cleanup.py` is not needed without LVGL. |
| Tests | `pio test -e native`: 13 tests covering the M1 criteria (§12) against real fixtures, including the midnight NDBC variant and the NWS marine parsing (D9) |
| Accuracy audit | `tools/audit.py` (device on USB + Wi-Fi): independent recomputation of every displayed field from live sources |
| Wi-Fi setup | On-device: when no network is saved (or by tapping/long-pressing the Wi-Fi status), a list of scanned networks and a touch keyboard. Credentials go to NVS. The Wi-Fi screen offers "‹ 返回" whenever cached data exists, so the display stays usable offline. |
| Fonts | Noto Sans SC (OFL) weights 500/700, subset to the characters used in the source; ▸/▴ were replaced (not in the font) |
| Milestones | M0–M3 done on the device on 2026-11-12. M4: the detail screen is built; the Chinese wording still needs the maintainer's review. M5: OTA works; the soak hasn't started. |
