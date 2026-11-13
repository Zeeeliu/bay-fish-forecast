#!/usr/bin/env python3
"""Generate smooth VLW fonts (Processing format, read by M5GFX/LovyanGFX) from Noto Sans SC, subset to the
characters the firmware uses (PRD §10.2).

usage: python3 tools/make_fonts.py            (from the repo root; writes data/fonts/*.vlw)
The font file comes from Google Fonts (OFL):  tools/fonts-src/NotoSansSC-VF.ttf
"""
import glob, os, struct, sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_FONT = os.path.join(ROOT, "tools", "fonts-src", "NotoSansSC-VF.ttf")
OUT = os.path.join(ROOT, "data", "fonts")

# name -> (pixel size, weight). Keep in sync with src/fonts.h.
FONTS = {
    "r16": (16, 500), "r17": (17, 500), "r18": (18, 500), "r20": (20, 500), "r21": (21, 500), "r24": (24, 500), "r26": (26, 500), "r28": (28, 500),
    "b16": (16, 700), "b18": (18, 700), "b20": (20, 700), "b21": (21, 700), "b24": (24, 700), "b25": (25, 700),
    "b26": (26, 700), "b28": (28, 700), "b32": (32, 700), "b38": (38, 700),
}
EXTRA = "—–−·…°›‹↑↓！，。：；、（）“”‘’「」？％＋～"


def charset():
    chars = set(chr(c) for c in range(32, 127)) | set(EXTRA)
    for pat in ("src/**/*.cpp", "src/**/*.h", "lib/core/**/*.cpp", "lib/core/**/*.h"):
        for path in glob.glob(os.path.join(ROOT, pat), recursive=True):
            with open(path, encoding="utf-8") as f:
                chars |= {ch for ch in f.read() if ord(ch) > 127 and ch.isprintable()}
    return sorted(chars, key=ord)


def build(name, size, weight, chars):
    font = ImageFont.truetype(SRC_FONT, size)
    font.set_variation_by_axes([weight])
    glyphs, bitmaps = [], []
    for ch in chars:
        x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        adv = int(round(font.getlength(ch)))
        if w and h:
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
            bmp = img.tobytes()
        else:
            bmp = b""
        glyphs.append((ord(ch), h, w, adv, -y0, x0))
        bitmaps.append(bmp)
    ascent = max(g[4] for g in glyphs)
    descent = max(g[1] - g[4] for g in glyphs)
    out = bytearray(struct.pack(">6i", len(glyphs), 11, size, 0, ascent, descent))
    for g in glyphs:
        out += struct.pack(">7i", *g, 0)
    for b in bitmaps:
        out += b
    with open(os.path.join(OUT, name + ".vlw"), "wb") as f:
        f.write(out)
    return len(out)


def main():
    if not os.path.exists(SRC_FONT):
        sys.exit("missing " + SRC_FONT + " (download NotoSansSC[wght].ttf from github.com/google/fonts/ofl/notosanssc)")
    os.makedirs(OUT, exist_ok=True)
    chars = charset()
    total = 0
    for name, (size, weight) in FONTS.items():
        total += build(name, size, weight, chars)
    print(f"{len(chars)} glyphs x {len(FONTS)} fonts -> {OUT} ({total / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
