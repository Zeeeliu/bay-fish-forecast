# PlatformIO post-script for [env:ota] (post: the platform resets UPLOADERFLAGS after pre-scripts): pass the OTA password without committing it.
# Order: $BAY_FISH_OTA_PASSWORD, then OTA_PASSWORD in include/secrets.h (gitignored).
import os, re
Import("env")
pw = os.environ.get("BAY_FISH_OTA_PASSWORD", "")
if not pw:
    p = os.path.join(env.subst("$PROJECT_DIR"), "include", "secrets.h")
    if os.path.exists(p):
        m = re.search(r'#define\s+OTA_PASSWORD\s+"([^"]*)"', open(p).read())
        pw = m.group(1) if m else ""
if pw:
    env.Append(UPLOADERFLAGS=["--auth=" + pw])
else:
    print("ota_auth: no OTA password (set BAY_FISH_OTA_PASSWORD or OTA_PASSWORD in include/secrets.h)")
