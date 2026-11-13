// Smooth Noto Sans SC fonts (VLW in LittleFS, loaded into PSRAM). Names match tools/make_fonts.py.
#pragma once
#include <M5GFX.h>

enum FontId { R16, R17, R18, R20, R21, R24, R26, R28, B16, B18, B20, B21, B24, B25, B26, B28, B32, B38, FONT_COUNT };

bool fontsBegin();                       // false if any VLW file is missing (falls back to efontCN)
const lgfx::IFont* font(FontId f);
bool fontsSmooth();
