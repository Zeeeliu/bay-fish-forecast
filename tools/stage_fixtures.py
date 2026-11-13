#!/usr/bin/env python3
"""TEST ONLY: copy the 2026-11-12 fixtures into data/cache/ so `pio run -t uploadfs` puts them on the device.
Remove data/cache/ and upload again afterwards (python3 tools/stage_fixtures.py --clean)."""
import json, os, shutil, sys
from datetime import datetime, timezone, timedelta
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "data", "cache")
if "--clean" in sys.argv:
    shutil.rmtree(OUT, ignore_errors=True); sys.exit(0)
PST = timezone(timedelta(hours=-8))
alert_at = int(datetime(2026, 11, 12, 6, 32, tzinfo=PST).timestamp())
fc_at = int(datetime(2026, 11, 12, 5, 12, tzinfo=PST).timestamp())
F = {
    "S1_halfmoon": "nws_alerts_point_hmb_2026-11-12.json", "S1_sf": "nws_alerts_point_sf_2026-11-12.json",
    "S1_redwood": "nws_alerts_point_rwc_2026-11-12.json", "S2_PZZ545": "nws_alerts_zone_PZZ545_2026-11-12.json",
    "S2_PZZ531": "nws_alerts_zone_PZZ531_2026-11-12.json",
    "S3_halfmoon": "openmeteo_forecast_hmb_2026-11-12.json", "S3_sf": "openmeteo_forecast_sf_2026-11-12.json",
    "S3_redwood": "openmeteo_forecast_rwc_2026-11-12.json", "S4_halfmoon": "openmeteo_marine_hmb_2026-11-12.json",
    "S4_sf": "openmeteo_marine_sf_2026-11-12.json",
    "S5_9414131": "coops_hilo_9414131_20261111-20261117.json", "S5_9414290": "coops_hilo_9414290_20261111-20261117.json",
    "S5_9414523": "coops_hilo_9414523_20261111-20261117.json",
    "S6_46012": "ndbc_latest_obs_46012_2026-11-12.txt", "S6_46026": "ndbc_latest_obs_46026_2026-11-12.txt",
    "S7_9414750": "coops_watertemp_9414750_latest.json",
}
os.makedirs(OUT, exist_ok=True)
meta = {}
for k, f in F.items():
    shutil.copy(os.path.join(ROOT, "fixtures", f), os.path.join(OUT, k + ".txt"))
    meta[k] = {"okAt": alert_at if k[:2] in ("S1", "S2") else fc_at}
json.dump(meta, open(os.path.join(OUT, "meta.json"), "w"))
print(f"staged {len(F)} responses in {OUT}")
