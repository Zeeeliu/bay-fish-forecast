#!/usr/bin/env python3
"""Accuracy audit: compare the device's Report (serial `report`) with values recomputed here, independently of the C++ code,
from the same public sources fetched at (nearly) the same time.  usage: tools/audit.py  (device on USB, Wi-Fi up)"""
import json, math, subprocess, sys, time, urllib.request
from datetime import datetime, timedelta, timezone

UA = {"User-Agent": "(bay-fish-forecast audit, https://github.com/Zeeeliu/bay-fish-forecast)", "Accept": "application/geo+json"}
PT = None
try:
    from zoneinfo import ZoneInfo
    PT = ZoneInfo("America/Los_Angeles")
except Exception:
    sys.exit("needs zoneinfo")

PLACES = [
    dict(id="halfmoon", land=(37.4636, -122.4286), zone="PZZ545", tide="9414131", buoy="46012", fc=(37.4636, -122.4286), mr=(37.48, -122.52), coastal=True),
    dict(id="sf", land=(37.76, -122.51), zone="PZZ545", tide="9414290", buoy="46026", fc=(37.76, -122.51), mr=(37.76, -122.54), coastal=True),
    dict(id="redwood", land=(37.4852, -122.2364), zone="PZZ531", tide="9414523", buoy="", fc=(37.4852, -122.2364), mr=(37.55, -122.22), coastal=False),
    dict(id="santacruz", land=(36.969, -122.023), zone="PZZ535", tide="9413745", buoy="46236", fc=(36.969, -122.023), mr=(36.93, -122.03), coastal=True),
]
PRIO = {"Tsunami Warning": 100, "Tsunami Advisory": 95, "Tsunami Watch": 90, "Storm Warning": 85, "Special Marine Warning": 84, "High Surf Warning": 82,
        "Hazardous Seas Warning": 80, "Gale Warning": 78, "Coastal Flood Warning": 76, "High Wind Warning": 74, "Beach Hazards Statement": 70,
        "High Surf Advisory": 68, "Rip Current Statement": 66, "Small Craft Advisory": 64, "Coastal Flood Advisory": 62, "Wind Advisory": 60,
        "Dense Fog Advisory": 58, "Coastal Flood Statement": 50}
DIRS = ["北", "东北", "东", "东南", "南", "西南", "西", "西北"]

def get(url, js=True):
    r = urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=20).read()
    return json.loads(r) if js else r.decode("latin-1")

def tab5(*args):
    out = subprocess.run(["tools/tab5", *args], capture_output=True, text=True, timeout=120).stdout
    return [l for l in out.splitlines() if l.startswith("{")]

COMPASS = {"N": "北", "NNE": "北", "NE": "东北", "ENE": "东", "E": "东", "ESE": "东", "SE": "东南", "SSE": "南", "S": "南", "SSW": "南",
           "SW": "西南", "WSW": "西", "W": "西", "WNW": "西", "NW": "西北", "NNW": "北", "Variable": "风向不定"}
WDAY = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]

def marine_days(doc):
    """Independent re-implementation (regex) of the NWS marine-zone summary used on the cards."""
    import re
    upd = datetime.fromisoformat(doc["updated"]).astimezone(PT).date()
    day, night = {}, {}
    for p in doc["periods"]:
        n, txt = p["name"], p["detailedForecast"].split("...")[0]
        if n in ("Today", "This Afternoon", "Rest of Today", "Tonight", "Overnight"): d = upd
        else:
            k = WDAY.index(n[:3]); d = next(upd + timedelta(days=a) for a in range(1, 8) if (upd + timedelta(days=a)).weekday() == k)
        isn = "Night" in n or n in ("Tonight", "Overnight")
        wind = [x for x in re.split(r"(?<=[a-z0-9])\. ", txt) if " kt" in x and not x.startswith(("Seas", "Waves", "Wave Detail")) and "current" not in x]
        best, gust = None, -1
        for sent in wind:
            g = re.findall(r"[Gg]usts up to (\d+) kt", sent)
            gust = max([gust] + [int(x) for x in g])
            sent = re.sub(r"[Gg]usts up to \d+ kt", "", sent)
            for m in re.finditer(r"(?:(\d+) to )?(\d+) kt", sent):
                dirs = re.findall(r"\b(N|NNE|NE|ENE|E|ESE|SE|SSE|S|SSW|SW|WSW|W|WNW|NW|NNW|Variable)\b", sent[:m.start()])
                hi = int(m.group(2)); lo = int(m.group(1)) if m.group(1) else hi
                if best is None or hi > best[1]: best = (lo, hi, dirs[-1] if dirs else "")
        seas = re.search(r"(?:Seas|Waves) ([^.]*)", txt)
        seas_hi = max(int(x) for x in re.findall(r"(\d+) ft", seas.group(1))) if seas else None
        det = re.findall(r"(\d+) ft at (\d+) seconds", txt.split("Wave Detail:")[1]) if "Wave Detail:" in txt else []
        per = int(max(det, key=lambda x: int(x[0]))[1]) if det else None
        (night if isn else day).setdefault(d.isoformat(), (best, gust, seas_hi, per))
    for k, v in night.items(): day.setdefault(k, v)
    return day

def marine_wind(best, gust):
    lo, hi, d = best
    s = COMPASS.get(d, "")
    s += (" " if s else "") + (f"{lo}–" if lo < hi else "") + f"{hi}节"
    return s + (f" 阵{gust}" if gust > 0 else "")

def wdir(d): return DIRS[int(((d % 360) + 22.5) // 45) % 8]
def r0(x): return int(math.floor(x + 0.5)) if x >= 0 else -int(math.floor(-x + 0.5))

results = []
def check(place, field, device, expected, note="", tol=0):
    ok = str(device) == str(expected)
    if not ok and tol:   # Open-Meteo serves two model runs from different servers for a while after an update: allow ±tol per number
        import re
        dn, en_ = re.findall(r"-?\d+", str(device)), re.findall(r"-?\d+", str(expected))
        ok = len(dn) == len(en_) and all(abs(int(x) - int(y)) <= tol for x, y in zip(dn, en_))
        note = f"within ±{tol} (Open-Meteo servers can lag a model run)"
    results.append((ok, place, field, device, expected, note))

def main():
    tab5("fetch")
    t0 = time.time()
    src = {}
    for p in PLACES:
        lat, lon = p["land"]
        src["S1_" + p["id"]] = get(f"https://api.weather.gov/alerts/active?point={lat},{lon}")
        src["S3_" + p["id"]] = get(f"https://api.open-meteo.com/v1/forecast?latitude={p['fc'][0]}&longitude={p['fc'][1]}&daily=weather_code,temperature_2m_max,temperature_2m_min,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant,sunrise,sunset&current=temperature_2m,wind_speed_10m,wind_gusts_10m,wind_direction_10m,weather_code&wind_speed_unit=kn&timezone=America/Los_Angeles&forecast_days=6")
        if p["coastal"]:
            src["S4_" + p["id"]] = get(f"https://marine-api.open-meteo.com/v1/marine?latitude={p['mr'][0]}&longitude={p['mr'][1]}&daily=wave_height_max,wave_period_max,wave_direction_dominant&current=wave_height,wave_period,sea_surface_temperature&timezone=America/Los_Angeles&forecast_days=6")
            src["S6_" + p["buoy"]] = get(f"https://www.ndbc.noaa.gov/data/latest_obs/{p['buoy']}.txt", js=False)
        now = datetime.now(PT)
        b, e = (now - timedelta(days=1)).strftime("%Y%m%d"), (now + timedelta(days=5)).strftime("%Y%m%d")
        src["S5_" + p["tide"]] = get(f"https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=predictions&application=audit&station={p['tide']}&begin_date={b}&end_date={e}&datum=MLLW&time_zone=lst_ldt&interval=hilo&units=metric&format=json")
    for z in sorted({p["zone"] for p in PLACES}):
        src["S2_" + z] = get(f"https://api.weather.gov/alerts/active?zone={z}")
    for z in sorted({p["zone"] for p in PLACES}):
        src["S8_" + z] = json.loads(urllib.request.urlopen(urllib.request.Request(f"https://api.weather.gov/zones/forecast/{z}/forecast", headers={**UA, "Accept": "application/ld+json"}), timeout=20).read())
    src["S7"] = get("https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=water_temperature&date=latest&station=9414750&units=metric&time_zone=lst_ldt&format=json&application=audit")
    # wait for the device's full refresh to finish (its last_full_refresh time changes), then read its report
    def status():
        for l in tab5("status"):
            try: return json.loads(l)["data"]
            except ValueError: pass
    start = (status() or {}).get("last_full_refresh")
    for _ in range(60):
        st = status()
        if st and st.get("last_full_refresh") and (st["last_full_refresh"] != start or time.time() - t0 > 90) and not st["wifi"]["status"].startswith("正在"):
            break
        time.sleep(3)
    time.sleep(3)
    rep = json.loads(tab5("report")[0])
    now = datetime.now(PT)
    today = now.date()

    for p, dp in zip(PLACES, rep["places"]):
        pid = p["id"]
        # ---------- alerts ----------
        feats = {f["properties"]["id"]: f["properties"] for k in ("S1_" + pid, "S2_" + p["zone"]) for f in src[k]["features"]}
        live = []
        for a in feats.values():
            vt = (a["parameters"].get("VTEC") or [""])[0]
            if a["status"] != "Actual" or a["messageType"] == "Cancel" or ".CAN." in vt or ".EXP." in vt: continue
            st = datetime.fromisoformat(a["onset"] or a["effective"]); en = datetime.fromisoformat(a["ends"] or a["expires"])
            if en <= now: continue
            live.append((a["event"], st, en, st <= now < en))
        check(pid, "alert count", len(dp["alerts"]["items"]), len(live))
        check(pid, "alert events (sorted)", sorted(i["event"] for i in dp["alerts"]["items"]), sorted(x[0] for x in live))
        ine = sorted([x for x in live if x[3]], key=lambda x: -PRIO.get(x[0], 40))
        top = ine[0] if ine else min(live, key=lambda x: x[1]) if live else None
        if top:
            check(pid, "top alert", dp["alerts"]["items"][0]["event"], top[0])
            ends = top[2].astimezone(PT)
            check(pid, "top alert end", dp["alerts"]["banner"]["until"].split()[-1], ends.strftime("%H:%M"))
        # ---------- tide ----------
        preds = [(datetime.strptime(x["t"], "%Y-%m-%d %H:%M").replace(tzinfo=PT), float(x["v"]), x["type"]) for x in src["S5_" + p["tide"]]["predictions"]]
        todays = [x for x in preds if x[0].date() == today]
        dev = [e for e in dp["tide"]["extremes"] if 0 <= e[0] < 1440]
        check(pid, "tide extremes today", [(e[0] // 60 * 100 + e[0] % 60, round(e[1], 3), e[2]) for e in dev],
              [(x[0].hour * 100 + x[0].minute, round(x[1], 3), x[2]) for x in todays])
        check(pid, "tide range", round(dp["tide"]["rangeM"], 2), round(max(x[1] for x in todays) - min(x[1] for x in todays), 2))
        # ---------- forecast ----------
        fc = src["S3_" + pid]; d = fc["daily"]; c = fc["current"]
        mar = marine_days(src["S8_" + p["zone"]])
        i0 = d["time"].index(today.isoformat())
        check(pid, "sunrise", dp["tide"]["sunrise"], d["sunrise"][i0][-5:])
        check(pid, "wind now", dp["tiles"]["wind"]["value"], f"{wdir(c['wind_direction_10m'])} {r0(c['wind_speed_10m'])}", "model value; may move between the two fetches")
        check(pid, "temp today", dp["tiles"]["weather"]["value"], f"{r0(d['temperature_2m_min'][i0])}–{r0(d['temperature_2m_max'][i0])}°C", tol=1)
        for k in range(5):
            i = i0 + k; day = dp["days"][k]
            check(pid, f"day{k} date", day["date"], d["time"][i])
            check(pid, f"day{k} temp", day["temp"], f"{r0(d['temperature_2m_min'][i])}–{r0(d['temperature_2m_max'][i])}°C", tol=1 if k < 3 else 2)
            w, g = r0(d["wind_speed_10m_max"][i]), r0(d["wind_gusts_10m_max"][i])
            mw = mar.get(d["time"][i])
            if mw and mw[0]:
                check(pid, f"day{k} wind (NWS marine)", day["wind"], marine_wind(mw[0], mw[1]))
            else:
                check(pid, f"day{k} wind (model)", day["wind"], f"{wdir(d['wind_direction_10m_dominant'][i])} {w}节{' 阵' + str(g) if g > w else ''}")
            dd = datetime.fromisoformat(d["time"][i]).date()
            hi = max((x for x in preds if x[0].date() == dd and x[2] == "H"), key=lambda x: x[1], default=None)
            lo = min((x for x in preds if x[0].date() == dd and x[2] == "L"), key=lambda x: x[1], default=None)
            check(pid, f"day{k} tide", day["tide"], f"满{hi[0]:%H:%M} 干{lo[0]:%H:%M}")
            if p["coastal"]:
                m = src["S4_" + pid]["daily"]
                if mw and mw[2] is not None:
                    check(pid, f"day{k} seas (NWS marine)", day["row2"], f"{mw[2] * 0.3048:.1f}米" + (f" · {mw[3]}秒" if mw[3] else ""))
                else:
                    check(pid, f"day{k} waves (model)", day["row2"], f"{m['wave_height_max'][i]:.1f}米 · {r0(m['wave_period_max'][i])}秒")
            else:
                check(pid, f"day{k} tidal range", day["row2"], f"{hi[1] - lo[1]:.1f}米")
        # ---------- buoy / water ----------
        if p["coastal"]:
            txt = src["S6_" + p["buoy"]]
            kv = {l.split(":")[0]: l.split(":", 1)[1] for l in txt.splitlines() if ":" in l and not l[0].isdigit()}
            ft = float((kv.get("Seas") or kv.get("Swell") or " nan").split()[0])
            if not math.isnan(ft):
                check(pid, "wave now (buoy, m)", dp["tiles"]["wave"]["value"], f"{ft * 0.3048:.1f}")
            if "Water Temp" in kv:
                f = float(kv["Water Temp"].split()[0])
                check(pid, "water temp (buoy, °C)", dp["tiles"]["water"]["value"], f"{r0((f - 32) * 5 / 9)}°C")
        else:
            check(pid, "water temp (Alameda, °C)", dp["tiles"]["water"]["value"], f"{r0(float(src['S7']['data'][-1]['v']))}°C")

    bad = [r for r in results if not r[0]]
    for ok, pl, f, dv, ex, note in results:
        print(f"{'OK  ' if ok else 'DIFF'} {pl:9} {f:28} device={dv}  expected={ex}  {note if (not ok or dv != ex) else ''}")
    print(f"\n{len(results) - len(bad)}/{len(results)} fields match")

if __name__ == "__main__":
    main()
