import json, re, sys
EVENTS = ["Beach Hazards Statement","High Surf Warning","High Surf Advisory","Coastal Flood Advisory","Coastal Flood Warning",
          "Coastal Flood Statement","Rip Current Statement","Small Craft Advisory","Gale Warning","Storm Warning","Hazardous Seas Warning"]
FT = 0.3048
def unwrap(t): return re.sub(r"\s*\n\s*(?!\n)", " ", t or "").strip()
def sections(desc):                      # "* WHAT...text" blocks
    return {m.group(1).strip(): unwrap(m.group(2)) for m in re.finditer(r"\*\s*([A-Z /]+?)\.\.\.(.*?)(?=\n\s*\*\s*[A-Z /]+?\.\.\.|\Z)", desc, re.S)}
def per_event(text):                     # "For the [first|second] <Event>, ..." clauses
    pat = r"For the (?:(first|second|third) )?(%s),\s*" % "|".join(map(re.escape, EVENTS))
    parts = re.split(pat, text); out = []
    for i in range(1, len(parts), 3): out.append((parts[i+1], parts[i] or "", parts[i+2].strip()))
    return out or [(None, "", text)]
def hhmm(t):
    h, rest = t.split(":"); mm, ap = rest.split(); h = int(h) % 12 + (12 if ap == "PM" else 0); return f"{h:02d}:{mm}"
def m2(a,b): return f"{float(a)*FT:.1f}–{float(b)*FT:.1f}米（{a}–{b}英尺）"
FACTS = [  # regex -> Chinese template
 (r"breaking waves of (\d+) to (\d+) ft", lambda m: "破浪 " + m2(m[1], m[2])),
 (r"occasional sets of (\d+) to (\d+) ft", lambda m: "偶有 " + m2(m[1], m[2]) + " 的大浪组"),
 (r"[Uu]p to ([\d.]+) ft of inundation", lambda m: f"低洼处可能积水深达 {float(m[1])*FT:.1f}米（{m[1]}英尺）"),
 (r"([\d.]+) ft above normal \(([\d.]+) ft MLLW\) at ([\d:]+ [AP]M) (\w+) (\d+/\d+)", lambda m: f"{m[5]} {hhmm(m[3])} 高潮约 {float(m[2])*FT:.1f}米（比正常高 {float(m[1])*FT:.1f}米）"),
 (r"seas (\d+) to (\d+) ft", lambda m: "海浪 " + m2(m[1], m[2])),
 (r"winds? (\d+) to (\d+) kt", lambda m: f"风速 {m[1]}–{m[2]} 节"),
]
KEYWORDS = [("sneaker wave", "疯狗浪"), ("rip current", "离岸流"), ("over seawalls", "浪越过防波堤"), ("road closure", "道路封闭"),
            ("jetties", "勿上防波堤"), ("turn your back on the ocean", "勿背对大海"), ("remain out of the water", "勿下水")]
def facts(text): return [f(m) for p, f in FACTS for m in re.finditer(p, text)]
def chips(text): l = unwrap(text).lower(); return [zh for en, zh in KEYWORDS if en in l]

d = json.load(open(sys.argv[1])); done = set()
ORD = {"": 0, "first": 0, "second": 1, "third": 2}
rank = {}
for f in sorted(d["features"], key=lambda f: f["properties"]["onset"]):
    p = f["properties"]; same = [g for g in d["features"] if g["properties"]["event"] == p["event"] and g["properties"]["description"] == p["description"]]
    rank[p["id"]] = sorted(same, key=lambda g: g["properties"]["onset"]).index(f)
for f in d["features"]:
    p = f["properties"]; vtec = p["parameters"].get("VTEC", [""])[0]
    mv = re.match(r"/O\.(\w+)\.\w+\.(\w+\.\w)\.", vtec); act, phen = (mv.group(1), mv.group(2)) if mv else ("?", "?")
    print(f"\n### {p['event']}  VTEC action={act} code={phen}  {p['onset']} → {p['ends']}  zones={[z.split('/')[-1] for z in p['affectedZones']]}")
    secs = sections(p["description"]); what = secs.get("WHAT", "")
    cl = [(o, t) for ev, o, t in per_event(what) if ev == p["event"]]
    mine = [t for o, t in cl if len(cl) == 1 or ORD[o] == rank[p["id"]]]
    for t in mine: print("  WHAT(own clause):", t[:110]); print("  facts:", facts(t))
    key = p["description"]
    if key not in done:
        done.add(key); print("  sections found:", list(secs))
        print("  ADDITIONAL facts:", facts(secs.get("ADDITIONAL DETAILS", "")))
        print("  chips:", chips(p["description"] + " " + (p["instruction"] or "")))
