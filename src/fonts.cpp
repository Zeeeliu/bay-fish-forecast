#include "fonts.h"
#include <LittleFS.h>

static const char* kNames[FONT_COUNT] = {"r16", "r17", "r18", "r20", "r21", "r24", "r26", "r28", "b16", "b18", "b20", "b21", "b24", "b25", "b26", "b28", "b32", "b38"};
static const lgfx::IFont* gFonts[FONT_COUNT];
static bool gSmooth = true;

static const lgfx::IFont* fallback(FontId f) {
  switch (f) {
    case R16: case R17: case B16: return &fonts::efontCN_16;
    case R18: case B18: case R20: case B20: case R21: case B21: return &fonts::efontCN_16_b;
    default: return &fonts::efontCN_24_b;
  }
}

bool fontsBegin() {
  bool all = true;
  for (int i = 0; i < FONT_COUNT; i++) {
    gFonts[i] = nullptr;
    File f = LittleFS.open(String("/fonts/") + kNames[i] + ".vlw", "r");
    if (f) {
      size_t n = f.size();
      uint8_t* buf = (uint8_t*)ps_malloc(n);
      if (buf && f.read(buf, n) == n) {
        auto* w = new lgfx::PointerWrapper();
        w->set(buf, n);
        auto* v = new lgfx::VLWfont();
        if (v->loadFont(w)) gFonts[i] = v;
      }
      f.close();
    }
    if (!gFonts[i]) { gFonts[i] = fallback((FontId)i); all = false; }
  }
  gSmooth = all;
  Serial.printf("fonts: %s\n", all ? "all VLW fonts loaded" : "some VLW fonts missing -> efontCN fallback (run tools/make_fonts.py + uploadfs)");
  return all;
}

const lgfx::IFont* font(FontId f) { return gFonts[f]; }
bool fontsSmooth() { return gSmooth; }
