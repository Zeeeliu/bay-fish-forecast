// UI (PRD §9, Prototype 2): S1 main, tide chart, S2 alert detail, S3 splash, Wi-Fi setup with on-screen keyboard.
#include <M5Unified.h>
#include <Preferences.h>
#include <cmath>
#include "app.h"
#include "fonts.h"
#include "format_zh.h"
#include "tz.h"

using namespace core;

// ---------- tokens (PRD §9.2) ----------
static const uint32_t BG = 0x0B1620, PANEL = 0x142433, PBORDER = 0x22384C, TEXT = 0xEEF3F7, TSUB = 0xC9D6E2, TMUTED = 0x9FB2C3,
                      ACCENT = 0x7FD3F2, SUN = 0xF5C451, ALERT = 0xF5A524, ALERTTX = 0x1A1206, NEXTCHIP = 0xFFE2B0,
                      NONEBG = 0x16324A, NONEBD = 0x5EC3E8, UNKBG = 0x2B2226, UNKBD = 0xF0937A, UNKTITLE = 0xF7B8A6, UNKSUB = 0xE4D6D9,
                      NIGHT = 0x07111A, GRID = 0x2A4258, AXIS = 0x3A5670, TABBD = 0x2E4A63, ERR = 0xF0937A;

static M5Canvas cv(&M5.Display);
enum class Screen { Main, Detail, WifiList, WifiPass, Splash };
static Screen screen = Screen::Splash;
static int tab = 0, sel = 0, engPage = 0;
static bool engOpen = false;
static Report rep;            // Chinese: detail page, Wi-Fi screens, and the landing page in Chinese mode
static Report repEn;          // English landing page (PRD D11)
static bool en = false;       // landing-page language, persisted in NVS
static Report& cur() { return en ? repEn : rep; }
static const char* tr(const char* zh, const char* e) { return en ? e : zh; }
static bool haveRep = false;
static uint32_t lastBuild = 0, seenVersion = ~0u, lastTouch = 0, drawnHash = 0;
static int lastMinute = -1;
static bool dirty = true;
static Preferences uiPrefs;
static bool anyData();

struct Rect { int x, y, w, h; bool hit(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; } };
static Rect tabRects[8], langRect, bannerRect, backRect, engRect, pgPrev, pgNext, statusRect, dateRect, rescanRect, wifiBackRect;
static std::vector<Rect> listRects;

// ---------- text helpers ----------
static uint32_t mix(uint32_t a, uint32_t b, float t) {   // a over b with alpha t
  auto c = [&](int s) { return (uint32_t)(((a >> s) & 255) * t + ((b >> s) & 255) * (1 - t)) << s; };
  return c(16) | c(8) | c(0);
}
static int tw(const std::string& s, FontId f) { cv.setFont(font(f)); return cv.textWidth(s.c_str()); }
static int txt(const std::string& s, int x, int y, FontId f, uint32_t col, textdatum_t d = textdatum_t::middle_left, uint32_t bg = 0, bool useBg = false) {
  cv.setFont(font(f));
  cv.setTextDatum(d);
  if (useBg) cv.setTextColor(col, bg); else cv.setTextColor(col);
  cv.drawString(s.c_str(), x, y);
  return cv.textWidth(s.c_str());
}
static size_t u8next(const std::string& s, size_t i) {
  unsigned char c = s[i];
  return i + (c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4);
}
static std::string ellipsize(const std::string& s, FontId f, int maxW) {
  if (tw(s, f) <= maxW) return s;
  std::string out;
  for (size_t i = 0; i < s.size();) {
    size_t j = u8next(s, i);
    std::string t = out + s.substr(i, j - i);
    if (tw(t + "…", f) > maxW) break;
    out = t; i = j;
  }
  return out + "…";
}
static std::vector<std::string> wrap(const std::string& s, FontId f, int maxW) {
  std::vector<std::string> lines;
  std::string line;
  size_t i = 0;
  while (i < s.size()) {
    if (s[i] == '\n') { lines.push_back(line); line.clear(); i++; continue; }
    size_t j = i;
    if ((unsigned char)s[i] < 0x80 && s[i] != ' ') { while (j < s.size() && (unsigned char)s[j] < 0x80 && s[j] != ' ' && s[j] != '\n') j++; }   // ASCII word
    else j = u8next(s, i);
    if (j < s.size() && s[j] == ' ') j++;
    std::string tok = s.substr(i, j - i);
    // CJK line breaking: closing punctuation never starts a line (it may overhang by one glyph instead)
    static const char* noStart[] = {"。", "，", "、", "；", "：", "！", "？", "）", "」", "』", "…", "·", ")", ",", "."};
    bool closing = false;
    for (auto c : noStart) if (tok == c) closing = true;
    if (!line.empty() && !closing && tw(line + tok, f) > maxW) { lines.push_back(line); line.clear(); if (tok == " ") { i = j; continue; } }
    line += tok;
    i = j;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}
static void box(int x, int y, int w, int h, int r, uint32_t fill, uint32_t border = 0, int bw = 0) {
  if (bw) { cv.fillSmoothRoundRect(x, y, w, h, r, border); cv.fillSmoothRoundRect(x + bw, y + bw, w - 2 * bw, h - 2 * bw, std::max(0, r - bw), fill); }
  else cv.fillSmoothRoundRect(x, y, w, h, r, fill);
}
static void line(int x0, int y0, int x1, int y1, float width, uint32_t c) { cv.drawWideLine(x0, y0, x1, y1, width / 2, c); }
static void dashed(int x0, int x1, int y, uint32_t c, int on = 4, int off = 6) { for (int x = x0; x < x1; x += on + off) cv.drawFastHLine(x, y, std::min(on, x1 - x), c); }

// ---------- icons (stroke style of the mockup, 24-unit grid scaled) ----------
static void icon(const std::string& n, int cx, int cy, int size, uint32_t c, uint32_t bg = 0x142433) {
  float s = size / 24.0f;
  auto P = [&](float x, float y, float x2, float y2, float w = 2) { line(cx + (x - 12) * s, cy + (y - 12) * s, cx + (x2 - 12) * s, cy + (y2 - 12) * s, w * s, c); };
  auto ring = [&](float x, float y, float r, float w = 2, float a0 = 0, float a1 = 360) {
    cv.fillArc(cx + (x - 12) * s, cy + (y - 12) * s, (r - w / 2) * s, (r + w / 2) * s, a0, a1, c);
  };
  if (n == "sun") {
    ring(12, 12, 4);
    for (int k = 0; k < 8; k++) { float a = k * M_PI / 4; P(12 + 7 * cos(a), 12 + 7 * sin(a), 12 + 9.5 * cos(a), 12 + 9.5 * sin(a)); }
  } else if (n == "cloud" || n == "rain" || n == "partly" || n == "storm") {
    float dy = (n == "rain" || n == "storm") ? -3 : 0;
    if (n == "partly") { cv.fillSmoothCircle(cx + (7 - 12) * s, cy + (8 - 12) * s, 4 * s, SUN); }
    auto blob = [&](float grow, uint32_t col) {   // cloud silhouette; drawn twice (stroke colour, then background) = outline
      cv.fillSmoothCircle(cx + (8.5 - 12) * s, cy + (14.5 + dy - 12) * s, (4.5 + grow) * s, col);
      cv.fillSmoothCircle(cx + (13 - 12) * s, cy + (11 + dy - 12) * s, (5.5 + grow) * s, col);
      cv.fillSmoothCircle(cx + (17.5 - 12) * s, cy + (14.5 + dy - 12) * s, (4 + grow) * s, col);
      cv.fillRect(cx + (8.5 - 12) * s, cy + (14.5 + dy - 12) * s, 9 * s, (4.5 + grow) * s, col);
    };
    blob(1, c);
    blob(-1, bg);
    if (n == "rain") { P(8, 19, 7, 22); P(12, 19, 11, 22); P(16, 19, 15, 22); }
    if (n == "storm") { P(13, 17, 11, 20); P(11, 20, 14, 20); P(14, 20, 12, 23); }
  } else if (n == "fog") {
    P(4, 8, 20, 8); P(2, 12, 22, 12); P(5, 16, 19, 16); P(8, 20, 16, 20);
  } else if (n == "wind") {
    P(3, 8, 14, 8); ring(14, 5, 3, 2, 270, 450); P(3, 12, 18, 12); ring(18, 15, 3, 2, -90, 90); P(3, 16, 10, 16);
  } else if (n == "wave") {
    for (int row = 0; row < 2; row++) for (int k = 0; k < 4; k++) ring(4 + k * 5, 9 + row * 6, 2.5, 2, k % 2 ? 0 : 180, k % 2 ? 180 : 360);
  } else if (n == "therm") {
    P(10, 4, 10, 14.8); P(14, 4, 14, 14.8); ring(12, 4, 2, 2, 180, 360); ring(12, 17.5, 4, 2, -50, 230);
  } else if (n == "drop") {
    P(12, 3, 6.5, 12); P(12, 3, 17.5, 12); ring(12, 14, 6, 2, -30, 210);
  } else if (n == "tide") {
    P(8, 20, 8, 6); P(4, 10, 8, 6); P(8, 6, 12, 10); P(16, 4, 16, 18); P(12, 14, 16, 18); P(16, 18, 20, 14);
  } else if (n == "warn") {
    P(12, 3, 2, 21, 2.2); P(12, 3, 22, 21, 2.2); P(2, 21, 22, 21, 2.2); P(12, 10, 12, 15, 2.4); cv.fillSmoothCircle(cx, cy + 6 * s, 1.4 * s, c);
  } else if (n == "info") {
    ring(12, 12, 9); P(12, 11, 12, 17); cv.fillSmoothCircle(cx, cy - 4.5 * s, 1.3 * s, c);
  } else if (n == "unknown") {
    ring(12, 12, 9); ring(12, 9, 2.6, 2, 180, 420); P(12, 12, 12, 13.5); cv.fillSmoothCircle(cx, cy + 5 * s, 1.3 * s, c);
  } else if (n == "wifi") {
    ring(12, 21, 16, 2, 225, 315); ring(12, 21, 11, 2, 225, 315); ring(12, 21, 6, 2, 225, 315); cv.fillSmoothCircle(cx, cy + 7.5 * s, 1.5 * s, c);
  }
}

// ---------- S1 pieces ----------
static void drawHeader(bool detail) {
  const Report& R = cur();
  cv.fillRect(0, 0, 1280, 64, BG);
  int x = 20;
  if (!detail) {
    int n = std::min((int)R.places.size(), 8);
    FontId tf = B24;
    int total = 0;
    for (int i = 0; i < n; i++) total += tw(R.places[i].name, tf) + 52 + 10;
    if (total > 760) tf = B21;   // English names are longer
    for (int i = 0; i < n; i++) {
      const std::string& name = R.places[i].name;
      int w = tw(name, tf) + (tf == B24 ? 52 : 36);
      tabRects[i] = {x, 15, w, 46};
      if (i == tab) box(x, 15, w, 46, 23, TEXT);
      else box(x, 15, w, 46, 23, BG, TABBD, 2);
      txt(name, x + w / 2, 38, tf, i == tab ? BG : TSUB, textdatum_t::middle_center);
      x += w + 10;
    }
    // language toggle: shows the language you switch TO
    const char* lab = en ? "中文" : "EN";
    int lw = tw(lab, B20) + 28;
    langRect = {x + 4, 19, lw, 38};
    box(langRect.x, 19, lw, 38, 19, PANEL, ACCENT, 2);
    txt(lab, langRect.x + lw / 2, 38, B20, ACCENT, textdatum_t::middle_center);
  }
  uint32_t sc = R.statusError ? ERR : TMUTED;
  std::string status = R.status;
  if (!app.wifiConnected && !wifiHaveCredentials()) status = tr("未设置 Wi-Fi · 点此设置", "Wi-Fi not set · tap");
  int sw = txt(status, 1260, 38, R20, sc, textdatum_t::middle_right);
  icon("wifi", 1260 - sw - 18, 38, 24, sc);
  statusRect = {1260 - sw - 34, 14, sw + 34, 48};
  std::string date = R.dateText + " " + hhmm(R.now);
  int dw = txt(date, 1260 - sw - 44, 38, B21, TEXT, textdatum_t::middle_right);
  dateRect = {1260 - sw - 44 - dw, 14, dw, 48};
}

static void drawBanner(const PlaceReport& p) {
  const int X = 20, Y = 72, W = 1240, H = 84;
  bannerRect = {X, Y, W, H};
  auto& a = p.alerts;
  if (a.state == AlertState::Issued) {
    box(X, Y, W, H, 14, ALERT);
    icon("warn", X + 40, Y + 42, 44, ALERTTX);
    // right block
    int uw = tw(a.banner.until, B20), bw = tw(tr("全文 ›", "Details ›"), B20) + 24;
    bool hasNext = !a.banner.next.empty();
    int nw = hasNext ? tw(a.banner.next, B18) + 20 : 0;
    int rw = std::max(uw + 10 + bw, nw);
    int rx = X + W - 18 - rw;
    int ry = hasNext ? Y + 26 : Y + 42;
    txt(a.banner.until, X + W - 18 - bw - 10, ry, B20, ALERTTX, textdatum_t::middle_right);
    box(X + W - 18 - bw, ry - 16, bw, 32, 8, ALERTTX);
    txt(tr("全文 ›", "Details ›"), X + W - 18 - bw / 2, ry, B20, ALERT, textdatum_t::middle_center);
    if (hasNext) { box(X + W - 18 - nw, Y + 48, nw, 28, 6, NEXTCHIP); txt(a.banner.next, X + W - 18 - nw / 2, Y + 62, B18, ALERTTX, textdatum_t::middle_center); }
    int tx = X + 80, maxW = rx - 16 - tx;
    int cw = a.banner.countText.empty() ? 0 : tw(a.banner.countText, B18) + 20;
    std::string title = ellipsize(a.banner.title, B26, maxW - cw - 12);
    int t = txt(title, tx, Y + 26, B26, ALERTTX);
    if (cw) { box(tx + t + 12, Y + 12, cw, 28, 14, ALERTTX); box(tx + t + 14, Y + 14, cw - 4, 24, 12, ALERT); txt(a.banner.countText, tx + t + 12 + cw / 2, Y + 26, B18, ALERTTX, textdatum_t::middle_center); }
    txt(ellipsize(a.banner.sub, R20, maxW), tx, Y + 62, R20, ALERTTX);
  } else if (a.state == AlertState::None) {
    box(X, Y, W, H, 14, NONEBG, NONEBD, 2);
    icon("info", X + 40, Y + 42, 42, ACCENT);
    txt(en ? "No applicable official alerts as of " + hhmm(a.checkedAt) : "截至 " + hhmm(a.checkedAt) + "，未查询到适用的官方警示", X + 80, Y + 26, B26, TEXT);
    txt(tr("未发布警示不等于安全。出海前请再次留意现场海况。", "No alert does not mean safe. Check conditions before going out."), X + 80, Y + 62, R20, TSUB);
  } else {
    cv.fillSmoothRoundRect(X, Y, W, H, 14, UNKBG);
    for (int x = X + 14; x < X + W - 14; x += 14) { cv.fillRect(x, Y, 8, 2, UNKBD); cv.fillRect(x, Y + H - 2, 8, 2, UNKBD); }
    for (int y = Y + 14; y < Y + H - 14; y += 14) { cv.fillRect(X, y, 2, 8, UNKBD); cv.fillRect(X + W - 2, y, 2, 8, UNKBD); }
    icon("unknown", X + 40, Y + 42, 42, UNKBD);
    txt(tr("警示状态未知：更新失败", "Alert status unknown: update failed"), X + 80, Y + 26, B26, UNKTITLE);
    txt(tr("无法确认是否有官方警示。出海前请查看 weather.gov 或 NOAA 天气广播。", "Cannot confirm official alerts. Check weather.gov or NOAA Weather Radio before going out."), X + 80, Y + 62, R20, UNKSUB);
  }
}

static void drawChart(const PlaceReport& p) {
  const int X = 20, Y = 166, W = 764, H = 254;
  box(X, Y, W, H, 14, PANEL, PBORDER, 1);
  icon("tide", X + 24, Y + 20, 20, ACCENT);
  txt(tr("今日潮汐 · ", "Today's tide · ") + p.tide.station, X + 40, Y + 20, R20, TMUTED);
  const TideDay& td = p.tide;
  if (td.ok) txt(tr("今日潮差 ", "Range ") + fmt2(td.rangeM) + tr("米", " m"), X + W - 14, Y + 20, B21, TEXT, textdatum_t::middle_right);
  const int ox = X + 14, oy = Y + 38;           // the mockup's 732 x 210 drawing area
  const int L = ox + 40, R = ox + 724, T = oy + 30, B = oy + 164;
  if (!td.ok) { txt(tr("潮汐数据不足", "Tide data unavailable"), X + W / 2, Y + H / 2 + 10, B24, TMUTED, textdatum_t::middle_center); return; }
  double maxH = -9;
  for (auto& e : td.extremes) if (e.minute > -360 && e.minute < td.dayMinutes + 360) maxH = std::max(maxH, e.h);
  const double vMin = -0.5, vMax = std::ceil(2 * maxH) / 2 + 0.2;
  auto Xm = [&](double m) { return L + (int)std::lround(m / td.dayMinutes * (R - L)); };
  auto Yv = [&](double v) { return T + (int)std::lround((vMax - v) / (vMax - vMin) * (B - T)); };
  int xr = td.sunriseMin >= 0 ? Xm(td.sunriseMin) : L, xs = td.sunsetMin >= 0 ? Xm(td.sunsetMin) : R;
  cv.fillRect(L, T, xr - L, B - T, NIGHT);
  cv.fillRect(xs, T, R - xs, B - T, NIGHT);
  for (int v = 0; v <= (int)vMax; v++) {
    int y = Yv(v);
    if (v == 0) cv.drawFastHLine(L, y, R - L, GRID); else dashed(L, R, y, GRID);
    txt(std::to_string(v) + tr("米", " m"), ox + 34, y, R16, TMUTED, textdatum_t::middle_right);
  }
  // area fill, one column at a time (16 % accent over night or panel)
  uint32_t fillDay = mix(ACCENT, PANEL, 0.16f), fillNight = mix(ACCENT, NIGHT, 0.16f);
  for (int x = L; x <= R; x++) {
    double m = (double)(x - L) / (R - L) * td.dayMinutes;
    int y = Yv(tideAt(td.extremes, m));
    cv.drawFastVLine(x, y, B - y, (x < xr || x >= xs) ? fillNight : fillDay);
  }
  int px = 0, py = 0;
  for (int m = 0; m <= td.dayMinutes; m += 10) {
    int x = Xm(m), y = Yv(tideAt(td.extremes, m));
    if (m) line(px, py, x, y, 3.5f, ACCENT);
    px = x; py = y;
  }
  cv.fillRect(L, B - 1, R - L, 2, AXIS);
  if (td.sunriseMin >= 0) txt(tr("日出 ", "Sunrise ") + td.sunrise, xr + 6, oy + 158, R16, SUN, textdatum_t::baseline_left);
  if (td.sunsetMin >= 0) txt(tr("日落 ", "Sunset ") + td.sunset, xs - 6, oy + 158, R16, SUN, textdatum_t::baseline_right);
  for (auto& e : td.extremes) {
    if (e.minute < 0 || e.minute >= td.dayMinutes) continue;
    int cx = std::min(std::max(Xm(e.minute), ox + 82), ox + 688), y = Yv(e.h);
    int y1 = e.high ? y - 26 : y - 34;
    int64_t t = localMidnight(rep.now) + (int64_t)e.minute * 60;
    txt(std::string(e.high ? tr("满 ", "High ") : tr("干 ", "Low ")) + hhmm(t), cx, y1, B18, e.high ? ACCENT : SUN, textdatum_t::baseline_center);
    txt(fmt2(e.h) + tr("米", " m"), cx, y1 + 18, R16, TSUB, textdatum_t::baseline_center);
  }
  int64_t mid = localMidnight(rep.now);
  LocalTime lt = toLocal(rep.now);
  for (int h : {0, 6, 12, 18, 24}) {
    int64_t at = fromLocal(lt.y, lt.mo, lt.d, h, 0, 0);
    int x = Xm((at - mid) / 60);
    txt(en ? (h < 10 ? "0" : "") + std::to_string(h) + ":00" : std::to_string(h) + "点", x, oy + 204, R16, TMUTED, h == 24 ? textdatum_t::baseline_right : textdatum_t::baseline_center);
  }
}

static void drawTile(int x, int y, const char* ic, const Tile& t) {
  const int w = 228, h = 122;
  box(x, y, w, h, 14, PANEL, PBORDER, 1);
  icon(ic, x + 24, y + 22, 20, ACCENT);
  txt(ellipsize(t.label, R18, w - 54), x + 40, y + 22, R18, TMUTED);
  int vw = txt(t.value, x + 14, y + 62, B38, TEXT);
  if (!t.unit.empty()) txt(t.unit, x + 14 + vw + 4, y + 68, R20, TEXT);
  txt(ellipsize(t.sub, R18, w - 28), x + 14, y + 100, R18, TSUB);
}

static void drawDay(int x, int y, const PlaceReport& p, const DayCard& d) {
  const int w = 240, h = 244;
  box(x, y, w, h, 14, PANEL, d.alert ? ALERT : PBORDER, 2);
  int dw = txt(d.day, x + 16, y + 26, B25, TEXT);
  txt(d.md, x + 16 + dw + 10, y + 28, R18, TMUTED);
  if (d.alert) { int tw2 = tw(tr("警示", "Alert"), B16) + 18; box(x + w - 16 - tw2, y + 13, tw2, 26, 13, ALERT); txt(tr("警示", "Alert"), x + w - 16 - tw2 / 2, y + 26, B16, ALERTTX, textdatum_t::middle_center); }
  uint32_t ic = d.icon == "sun" ? SUN : d.icon == "rain" ? ACCENT : TSUB;
  icon(d.icon, x + 34, y + 72, 34, ic);
  // temperature keeps priority; when "12–21°C" + a 3-character weather word don't fit, step the temperature, then the word, down a size
  const int tx0 = x + 58, avail = (x + w - 14) - tx0;
  FontId tf = B28, wf = R20;
  int ttw = tw(d.temp, tf), wxw = tw(d.wx, wf);
  if (ttw + 8 + wxw > avail) { tf = B26; ttw = tw(d.temp, tf); }
  if (ttw + 8 + wxw > avail) { wf = R18; wxw = tw(d.wx, wf); }
  if (ttw + 8 + wxw > avail) { tf = B24; ttw = tw(d.temp, tf); }   // English weather words are longer
  txt(d.temp, tx0, y + 72, tf, TEXT);
  txt(ellipsize(d.wx, wf, avail - ttw - 8), x + w - 14, y + 72, wf, TEXT, textdatum_t::middle_right);
  const char* labels[3] = {tr("风 ", "Wind "), nullptr, tr("潮 ", "Tide ")};
  std::string vals[3] = {d.wind, d.row2, d.tide};
  for (int i = 0; i < 3; i++) {
    std::string lab = i == 1 ? d.row2Label + " " : labels[i];
    int lw = txt(lab, x + 16, y + 122 + i * 42, R21, TMUTED);
    txt(ellipsize(vals[i], R21, w - 32 - lw), x + 16 + lw, y + 122 + i * 42, R21, TEXT);
  }
}

static void drawMain() {
  cv.fillScreen(BG);
  const PlaceReport& p = cur().places[tab];
  drawHeader(false);
  drawBanner(p);
  drawChart(p);
  bool coastal = p.profile == Profile::Coastal;
  drawTile(794, 166, "wind", p.wind);
  drawTile(1032, 166, coastal ? "wave" : "wave", p.wave);
  drawTile(794, 298, "therm", p.weather);
  drawTile(1032, 298, "drop", p.water);
  for (int i = 0; i < 5 && i < (int)p.days.size(); i++) drawDay(20 + i * 250, 430, p, p.days[i]);
  int srcW = txt(p.sourcesText, 1260, 695, R17, TMUTED, textdatum_t::middle_right);   // sources (incl. Open-Meteo attribution) always win
  txt(ellipsize(cur().footerLeft, R17, 1260 - srcW - 20 - 20), 20, 695, R17, cur().forecastStale ? ALERT : TMUTED);
}

// ---------- S2 alert detail (PRD §9.5; type enlarged after readability feedback 2026-11-12) ----------
static void drawDetail() {
  cv.fillScreen(BG);
  const PlaceReport& p = rep.places[tab];
  auto& items = p.alerts.items;
  std::string back = "‹ 返回 " + p.name;
  int bw = tw(back, B24) + 44;
  backRect = {20, 12, bw, 52};
  box(20, 12, bw, 52, 26, BG, TABBD, 2);
  txt(back, 20 + bw / 2, 38, B24, TEXT, textdatum_t::middle_center);
  txt("来源：美国国家气象局（NWS）", 1260, 38, R21, TMUTED, textdatum_t::middle_right);
  int top = 76;
  if (p.alerts.state == AlertState::Unknown) {
    box(20, top, 1240, 46, 10, UNKBG, UNKBD, 2);
    txt("以下为 " + (p.alerts.checkedAt ? hhmm(p.alerts.checkedAt) : std::string("—")) + " 的缓存信息，当前状态未知", 36, top + 23, R24, UNKTITLE);
    top += 56;
  }
  txt("中文为摘要说明，以英文原文为准 · 原文见 weather.gov", 20, 700, R20, TMUTED);
  if (items.empty()) { txt("暂无官方警示信息", 640, 360, B32, TMUTED, textdatum_t::middle_center); return; }
  if (sel >= (int)items.size()) sel = 0;
  int x = 20;
  listRects.clear();
  if (items.size() >= 2) {
    for (size_t i = 0; i < items.size() && i < 7; i++) {
      int y = top + (int)i * 82;
      Rect r{20, y, 320, 74};
      listRects.push_back(r);
      box(r.x, r.y, r.w, r.h, 12, PANEL, (int)i == sel ? ALERT : PBORDER, 2);
      cv.fillRect(r.x + 5, r.y + 12, 6, r.h - 24, items[i].inEffect ? ALERT : NEXTCHIP);
      txt(ellipsize(items[i].nameZh, B26, 285), r.x + 22, r.y + 24, B26, TEXT);
      txt(items[i].inEffect ? "生效中" : weekdayTime(items[i].start) + " 起", r.x + 22, r.y + 55, R21, items[i].inEffect ? ALERT : TSUB);
    }
    x = 356;
  }
  const Alert& a = items[sel];
  const int W = 1260 - x;
  int y = top + 6;
  int t1 = txt(a.nameZh, x, y + 20, B38, TEXT);
  int t2 = txt(a.event, x + t1 + 16, y + 26, R21, TMUTED);
  if (a.vtecAction == "NEW") { int w = tw("新发布", B20) + 20; box(x + t1 + t2 + 32, y + 8, w, 32, 16, NEXTCHIP); txt("新发布", x + t1 + t2 + 32 + w / 2, y + 24, B20, ALERTTX, textdatum_t::middle_center); }
  y += 62;
  int lw = txt("时间 ", x, y, B26, ALERT); txt(ellipsize(a.periodText + (a.inEffect ? "（生效中）" : ""), R26, W - lw), x + lw, y, R26, TEXT);
  y += 44;
  lw = txt("区域 ", x, y, B26, ALERT);
  auto area = wrap(a.areaText, R26, W - lw);
  for (size_t i = 0; i < area.size() && i < 2; i++) { txt(area[i], x + lw, y, R26, TEXT); y += 38; }
  y += 8;
  engRect = {x, 0, 0, 0};
  const int bottom = 672;
  if (engOpen) {
    auto it = p.alerts.englishByKey.find(a.descKey);
    std::string en = it == p.alerts.englishByKey.end() ? "" : it->second;
    auto lines = wrap(en, R20, W - 10);
    int per = std::max(1, (bottom - 56 - y) / 28);
    int pages = std::max(1, (int)((lines.size() + per - 1) / per));
    engPage = std::min(engPage, pages - 1);
    for (int i = 0; i < per && engPage * per + i < (int)lines.size(); i++) txt(lines[engPage * per + i], x, y + 10 + i * 28, R20, TSUB);
    int by = bottom - 26;
    engRect = {x, by - 24, tw("收起英文原文 ‹", B24) + 36, 48};
    box(engRect.x, engRect.y, engRect.w, engRect.h, 12, PANEL, PBORDER, 1);
    txt("收起英文原文 ‹", engRect.x + 18, by, B24, ACCENT);
    pgPrev = pgNext = {0, 0, 0, 0};
    if (pages > 1) {
      txt(std::to_string(engPage + 1) + " / " + std::to_string(pages), 1260 - 130, by, R21, TMUTED, textdatum_t::middle_center);
      pgPrev = {1260 - 230, by - 24, 70, 48}; pgNext = {1260 - 70, by - 24, 70, 48};
      box(pgPrev.x, pgPrev.y, 70, 48, 12, PANEL, PBORDER, 1); txt("‹", pgPrev.x + 35, by, B28, TEXT, textdatum_t::middle_center);
      box(pgNext.x, pgNext.y, 70, 48, 12, PANEL, PBORDER, 1); txt("›", pgNext.x + 35, by, B28, TEXT, textdatum_t::middle_center);
    }
    return;
  }
  txt("说明", x, y, B26, ALERT); y += 40;
  for (auto& l : wrap(a.fullTpl, R28, W)) { txt(l, x, y, R28, TEXT); y += 40; }
  if (!a.facts.empty()) {
    y += 8; txt("要点", x, y, B26, ALERT); y += 40;
    for (auto& f : a.facts) { if (y > bottom - 110) break; txt("• " + f, x, y, R28, TEXT); y += 40; }
  }
  if (!a.chips.empty() && y < bottom - 100) {
    y += 10;
    int cx = x;
    for (auto& c : a.chips) {
      int w = tw(c, R24) + 28;
      if (cx + w > 1260) { cx = x; y += 48; if (y > bottom - 90) break; }
      box(cx, y - 20, w, 40, 20, BG, TABBD, 2);
      txt(c, cx + w / 2, y, R24, TSUB, textdatum_t::middle_center);
      cx += w + 12;
    }
    y += 42;
  }
  if (!p.alerts.productFacts.empty() && y < bottom - 120) {
    y += 6; txt("补充", x, y, B26, ALERT); y += 38;
    for (auto& f : p.alerts.productFacts) { if (y > bottom - 80) break; txt("• " + f, x, y, R24, TSUB); y += 34; }
  }
  int by = bottom - 26;
  engRect = {x, by - 24, tw("英文原文 ›", B24) + 36, 48};
  box(engRect.x, engRect.y, engRect.w, engRect.h, 12, PANEL, PBORDER, 1);
  txt("英文原文 ›", engRect.x + 18, by, B24, ACCENT);
}

// ---------- S3 splash ----------
static void drawSplash(const char* msg) {
  cv.fillScreen(BG);
  txt("钓鱼海况", 640, 320, B38, TEXT, textdatum_t::middle_center);
  txt(msg, 640, 380, R21, TSUB, textdatum_t::middle_center);
}

// ---------- Wi-Fi setup ----------
static std::string kbSsid, kbText;
static bool kbShift = false, kbSym = false, kbShow = false;
struct Key { Rect r; std::string label, out; };
static std::vector<Key> keys;
static std::vector<WifiNet> scanCopy;

static void drawWifiList() {
  cv.fillScreen(BG);
  txt("Wi-Fi 设置", 20, 38, B32, TEXT);
  std::string cur, st;
  { Lock l; cur = app.wifiSsid; st = app.wifiStatus; scanCopy = app.scan; }
  txt("当前：" + (cur.empty() ? std::string("未设置") : cur) + " · " + st, 240, 40, R20, TSUB);
  rescanRect = {1260 - 170, 15, 170, 46};
  box(rescanRect.x, rescanRect.y, 170, 46, 23, BG, TABBD, 2);
  txt(app.scanning ? "扫描中…" : "重新扫描", rescanRect.x + 85, 38, B21, TEXT, textdatum_t::middle_center);
  wifiBackRect = {0, 0, 0, 0};
  if (wifiHaveCredentials() || (haveRep && anyData())) {
    wifiBackRect = {1260 - 170 - 140, 15, 130, 46};
    box(wifiBackRect.x, 15, 130, 46, 23, BG, TABBD, 2);
    txt("‹ 返回", wifiBackRect.x + 65, 38, B21, TEXT, textdatum_t::middle_center);
  }
  txt("选择网络（需 2.4 GHz）", 20, 92, R20, TMUTED);
  listRects.clear();
  for (size_t i = 0; i < scanCopy.size() && i < 8; i++) {
    int col = i % 2, row = i / 2;
    Rect r{20 + col * 625, 116 + row * 74, 615, 64};
    listRects.push_back(r);
    box(r.x, r.y, r.w, r.h, 12, PANEL, scanCopy[i].ssid == cur ? ACCENT : PBORDER, 2);
    txt(ellipsize(scanCopy[i].ssid, R24, 440), r.x + 20, r.y + 32, R24, TEXT);
    int bars = scanCopy[i].rssi > -55 ? 4 : scanCopy[i].rssi > -67 ? 3 : scanCopy[i].rssi > -78 ? 2 : 1;
    for (int b = 0; b < 4; b++) cv.fillRect(r.x + r.w - 70 + b * 12, r.y + 44 - (b + 1) * 7, 8, (b + 1) * 7, b < bars ? ACCENT : PBORDER);
    if (scanCopy[i].secure) txt("密码", r.x + r.w - 130, r.y + 32, R18, TMUTED, textdatum_t::middle_center);
  }
  if (scanCopy.empty()) txt(app.scanning ? "正在扫描附近的网络…" : "未找到网络，请点「重新扫描」", 640, 360, R24, TSUB, textdatum_t::middle_center);
}

static void drawWifiPass() {
  cv.fillScreen(BG);
  txt("输入「" + kbSsid + "」的密码", 20, 38, B26, TEXT);
  box(20, 80, 1000, 70, 12, PANEL, ACCENT, 2);
  std::string shown = kbShow ? kbText : std::string(kbText.size(), '*');
  txt(ellipsize(shown + "_", R24, 960), 40, 115, R24, TEXT);
  keys.clear();
  Rect showR{1040, 80, 220, 70};
  keys.push_back({showR, kbShow ? "隐藏密码" : "显示密码", "\x01"});
  const char* rowsA[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
  const char* rowsS[] = {"1234567890", "!@#$%^&*()", "-_=+[]{};:", "'\",.<>/?\\|"};
  const int kw = 112, kh = 70, g = 8, x0 = 44, y0 = 190;
  for (int r = 0; r < 4; r++) {
    std::string row = kbSym ? rowsS[r] : rowsA[r];
    int n = row.size();
    int rx = x0 + (r == 2 && !kbSym ? (kw + g) / 2 : 0) + (r == 3 && !kbSym ? (kw + g) * 3 / 2 : 0);
    for (int i = 0; i < n; i++) {
      std::string ch(1, row[i]);
      if (kbShift && !kbSym) ch[0] = toupper(ch[0]);
      keys.push_back({{rx + i * (kw + g), y0 + r * (kh + g), kw, kh}, ch, ch});
    }
  }
  int y4 = y0 + 3 * (kh + g);
  if (!kbSym) keys.push_back({{x0, y4, kw + kw / 2, kh}, kbShift ? "大写" : "Aa", "\x02"});
  keys.push_back({{x0 + 8 * (kw + g) + kw / 2 + g, y4, kw + kw / 2 + g, kh}, "删除", "\x08"});
  int y5 = y0 + 4 * (kh + g);
  keys.push_back({{x0, y5, 2 * kw + g, kh}, kbSym ? "abc" : "符号", "\x03"});
  keys.push_back({{x0 + 2 * (kw + g), y5, 4 * kw + 3 * g, kh}, "空格", " "});
  keys.push_back({{x0 + 6 * (kw + g), y5, 2 * kw + g, kh}, "取消", "\x1b"});
  keys.push_back({{x0 + 8 * (kw + g), y5, 2 * kw + g, kh}, "连接", "\r"});
  for (auto& k : keys) {
    bool special = k.out.size() == 1 && (k.out[0] < 0x20);
    uint32_t fill = k.out == "\r" ? ALERT : special ? 0x1C3247 : PANEL;
    box(k.r.x, k.r.y, k.r.w, k.r.h, 10, fill, PBORDER, 1);
    txt(k.label, k.r.x + k.r.w / 2, k.r.y + k.r.h / 2, k.label.size() == 1 ? B28 : B21, k.out == "\r" ? ALERTTX : TEXT, textdatum_t::middle_center);
  }
}

static void openWifi() { screen = Screen::WifiList; app.scanReq = true; dirty = true; }

// ---------- build / draw / touch ----------
static void rebuild() {
  Inputs in;
  { Lock l; in = app.inputs; }
  in.clockValid = app.clockValid;
  in.wifiConnected = app.wifiConnected;
  int64_t now = nowUtc();
  rep = buildReport(in, now, Lang::Zh);
  repEn = buildReport(in, now, Lang::En);
  haveRep = true;
}

static uint32_t reportHash() {
  std::string j = reportJson(rep) + reportJson(repEn) + (en ? "en" : "zh");
  uint32_t h = 2166136261u;
  for (unsigned char c : j) { h ^= c; h *= 16777619u; }
  return h;
}

static void drawAll() {
  if (screen == Screen::Splash) drawSplash(haveRep ? "正在加载…" : "钓鱼海况 · 正在连接…");
  else if (screen == Screen::Main) drawMain();
  else if (screen == Screen::Detail) drawDetail();
  else if (screen == Screen::WifiList) drawWifiList();
  else drawWifiPass();
  cv.pushSprite(0, 0);
}

static bool anyData() { for (auto& p : rep.places) if (!p.alerts.items.empty() || p.tide.ok || p.days[0].temp != "—") return true; return false; }

static void onTap(int x, int y) {
  lastTouch = millis();
  if (screen == Screen::Main) {
    for (int i = 0; i < (int)rep.places.size() && i < 8; i++) if (tabRects[i].hit(x, y)) { tab = i; sel = 0; uiPrefs.putInt("tab", tab); dirty = true; return; }
    if (langRect.hit(x, y)) { en = !en; uiPrefs.putBool("en", en); dirty = true; return; }
    if (bannerRect.hit(x, y) && rep.places[tab].alerts.state != AlertState::None) { screen = Screen::Detail; sel = 0; engOpen = false; dirty = true; return; }
    if (statusRect.hit(x, y) && !app.wifiConnected) { openWifi(); return; }
  } else if (screen == Screen::Detail) {
    if (backRect.hit(x, y)) { screen = Screen::Main; dirty = true; return; }
    for (size_t i = 0; i < listRects.size(); i++) if (listRects[i].hit(x, y)) { sel = i; engOpen = false; engPage = 0; dirty = true; return; }
    if (engRect.hit(x, y)) { engOpen = !engOpen; engPage = 0; dirty = true; return; }
    if (engOpen && pgPrev.hit(x, y)) { engPage = std::max(0, engPage - 1); dirty = true; return; }
    if (engOpen && pgNext.hit(x, y)) { engPage++; dirty = true; return; }
  } else if (screen == Screen::WifiList) {
    if (rescanRect.hit(x, y)) { app.scanReq = true; dirty = true; return; }
    if (wifiBackRect.hit(x, y)) { screen = Screen::Main; dirty = true; return; }
    for (size_t i = 0; i < listRects.size() && i < scanCopy.size(); i++)
      if (listRects[i].hit(x, y)) {
        kbSsid = scanCopy[i].ssid; kbText.clear(); kbShift = kbSym = kbShow = false;
        if (!scanCopy[i].secure) { Lock l; app.reqSsid = kbSsid; app.reqPass = ""; app.connectReq = true; screen = Screen::Main; }
        else screen = Screen::WifiPass;
        dirty = true; return;
      }
  } else if (screen == Screen::WifiPass) {
    for (auto& k : keys) {
      if (!k.r.hit(x, y)) continue;
      if (k.out == "\x01") kbShow = !kbShow;
      else if (k.out == "\x02") kbShift = !kbShift;
      else if (k.out == "\x03") kbSym = !kbSym;
      else if (k.out == "\x08") { if (!kbText.empty()) kbText.pop_back(); }
      else if (k.out == "\x1b") screen = Screen::WifiList;
      else if (k.out == "\r") { { Lock l; app.reqSsid = kbSsid; app.reqPass = kbText; app.connectReq = true; } screen = Screen::Main; }
      else { kbText += k.out; if (kbShift) kbShift = false; }
      dirty = true; return;
    }
  }
}

static void onHold(int x, int y) {
  lastTouch = millis();
  if (screen != Screen::Main) return;
  if (statusRect.hit(x, y)) { openWifi(); return; }               // long-press Wi-Fi status -> Wi-Fi settings
  if (dateRect.hit(x, y) || x > 900 && y < 64) { app.refreshReq = true; Lock l; app.inputs.refreshing = true; dirty = true; }   // PRD §2: manual refresh
}

void uiBegin() {
  uiPrefs.begin("ui", false);
  tab = std::min((int)core::places().size() - 1, std::max(0, (int)uiPrefs.getInt("tab", 0)));
  en = uiPrefs.getBool("en", false);
  M5.Display.setRotation(3);
  M5.Display.setBrightness(204);
  fontsBegin();
  cv.setPsram(true);
  cv.setColorDepth(16);
  if (!cv.createSprite(1280, 720)) Serial.println("ui: canvas allocation FAILED");
  screen = Screen::Splash;
  drawAll();
}

void uiLoop() {
  // touch
  auto t = M5.Touch.getDetail();
  if (t.wasClicked()) onTap(t.x, t.y);
  else if (t.wasHold()) onHold(t.x, t.y);
  if (t.isPressed()) lastTouch = millis();
  // brightness (PRD §10.7)
  static uint32_t lastBright = 0;
  if (millis() - lastBright > 1000) {
    lastBright = millis();
    LocalTime lt = toLocal(nowUtc());
    int mins = lt.h * 60 + lt.mi;
    bool day = !app.clockValid || (mins >= 5 * 60 && mins < 21 * 60 + 30);
    bool woke = millis() - lastTouch < 120000;
    M5.Display.setBrightness(day || woke ? 204 : 38);
  }
  // screen timeout for detail
  if (screen == Screen::Detail && millis() - lastTouch > 180000) { screen = Screen::Main; dirty = true; }
  // rebuild (PRD §7.7): every 30 s and after every fetch cycle
  if (app.dataVersion != seenVersion || millis() - lastBuild > 30000 || !haveRep) {
    seenVersion = app.dataVersion; lastBuild = millis();
    rebuild();
    if (screen == Screen::Splash) {
      if (!wifiHaveCredentials() && !app.wifiConnected) openWifi();
      else if (anyData()) screen = Screen::Main;
      dirty = true;
    }
    uint32_t h = reportHash();
    if (h != drawnHash) { drawnHash = h; if (screen == Screen::Main || screen == Screen::Detail) dirty = true; }
  }
  if (screen == Screen::WifiList) {
    static uint32_t lastScanDraw = 0;
    static bool wasScanning = false;
    if (wasScanning != app.scanning || millis() - lastScanDraw > 5000) { wasScanning = app.scanning; lastScanDraw = millis(); dirty = true; }
  }
  int minute = toLocal(nowUtc()).mi;
  if (dirty) { dirty = false; lastMinute = minute; drawAll(); }
  else if (minute != lastMinute && (screen == Screen::Main || screen == Screen::Detail)) {
    lastMinute = minute;
    rep.now = repEn.now = nowUtc();
    rep.dateText = dateHeader(rep.now);
    repEn.dateText = dateHeaderEn(rep.now);
    if (screen == Screen::Main) { drawHeader(false); cv.pushSprite(0, 0); }   // PRD §9.7: header-only redraw each minute
  }
}

// for serial `report`
std::string uiReportJson() { return haveRep ? reportJson(rep) : "{}"; }

// serial `shot`: the current canvas as a base64 PNG between markers (for checking the real rendering from the Mac)
void uiScreenshot() {
  size_t len = 0;
  uint8_t* png = (uint8_t*)cv.createPng(&len, 0, 0, 1280, 720);
  if (!png) { Serial.println("{\"ok\":false,\"error\":\"png failed\"}"); return; }
  static const char* B = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  Serial.printf("-----PNG %u-----\n", (unsigned)len);
  char out[77]; int o = 0;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t v = png[i] << 16 | (i + 1 < len ? png[i + 1] << 8 : 0) | (i + 2 < len ? png[i + 2] : 0);
    out[o++] = B[v >> 18 & 63]; out[o++] = B[v >> 12 & 63];
    out[o++] = i + 1 < len ? B[v >> 6 & 63] : '='; out[o++] = i + 2 < len ? B[v & 63] : '=';
    if (o == 76) { out[o] = 0; Serial.println(out); o = 0; }
  }
  if (o) { out[o] = 0; Serial.println(out); }
  Serial.println("-----END-----");
  free(png);
}

// serial `tap x y`: simulate a touch (testing)
void uiTap(int x, int y, bool hold) { if (hold) onHold(x, y); else onTap(x, y); }
