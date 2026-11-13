#include "alert_text.h"
#include "format_zh.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <sstream>

namespace core {

static const double FT = 0.3048;

std::string unwrap(const std::string& s) {
  std::string o;
  bool space = false;
  for (char c : s) {
    if (c == '\n' || c == '\r' || c == ' ' || c == '\t') { space = true; continue; }
    if (space && !o.empty()) o += ' ';
    space = false;
    o += c;
  }
  return o;
}

// "* WHAT...text" headings; hand-parsed (std::regex recursion per character overflows small stacks on the device)
static bool sectionHead(const std::string& line, std::string& name, std::string& rest) {
  size_t i = 0;
  while (i < line.size() && line[i] == ' ') i++;
  if (i >= line.size() || line[i] != '*') return false;
  i++;
  while (i < line.size() && line[i] == ' ') i++;
  size_t dots = line.find("...", i);
  if (dots == std::string::npos || dots == i) return false;
  for (size_t k = i; k < dots; k++) if (!(isupper((unsigned char)line[k]) || line[k] == ' ' || line[k] == '/')) return false;
  name = line.substr(i, dots - i);
  rest = line.substr(dots + 3);
  return true;
}

std::vector<Section> splitSections(const std::string& d) {
  std::vector<Section> out;
  std::istringstream in(d);
  std::string line, cur, name, rest;
  auto flush = [&]() { if (!out.empty()) out.back().text = unwrap(cur); cur.clear(); };
  while (std::getline(in, line)) {
    if (sectionHead(line, name, rest)) { flush(); out.push_back({name, ""}); cur = rest; }
    else if (!out.empty()) cur += "\n" + line;
  }
  flush();
  return out;
}

std::string sectionText(const std::vector<Section>& s, const std::string& name) {
  for (auto& x : s) if (x.name == name) return x.text;
  return "";
}

// "For the [first|second|third ]<Event>," markers; hand-parsed for the same stack reason
std::vector<Clause> eventClauses(const std::string& what) {
  static const std::vector<std::string> names = eventNames();
  struct Hit { size_t start, after; std::string event; int ordinal; };
  std::vector<Hit> hits;
  const std::string lead = "For the ";
  for (size_t p = what.find(lead); p != std::string::npos; p = what.find(lead, p + 1)) {
    size_t q = p + lead.size();
    int ord = 0;
    for (auto [w, o] : {std::pair<const char*, int>{"first ", 0}, {"second ", 1}, {"third ", 2}})
      if (what.compare(q, strlen(w), w) == 0) { ord = o; q += strlen(w); break; }
    for (auto& n : names) {
      if (what.compare(q, n.size(), n) == 0 && q + n.size() < what.size() && what[q + n.size()] == ',') {
        size_t a = q + n.size() + 1;
        while (a < what.size() && what[a] == ' ') a++;
        hits.push_back({p, a, n, ord});
        break;
      }
    }
  }
  std::vector<Clause> out;
  for (size_t i = 0; i < hits.size(); i++) {
    size_t b = i + 1 < hits.size() ? hits[i + 1].start : what.size();
    std::string t = what.substr(hits[i].after, b - hits[i].after);
    while (!t.empty() && t.back() == ' ') t.pop_back();
    out.push_back({hits[i].event, hits[i].ordinal, t});
  }
  return out;
}

static std::string m1(double ft) { char b[16]; snprintf(b, sizeof b, "%.1f", ft * FT); return b; }
static std::string range(const std::string& a, const std::string& b) {
  return m1(atof(a.c_str())) + "–" + m1(atof(b.c_str())) + "米（" + a + "–" + b + "英尺）";
}
static std::string to24(const std::string& t) {   // "12:10 PM" -> "12:10"
  int h = 0, mi = 0; char ap[3] = {0};
  if (sscanf(t.c_str(), "%d:%d %2s", &h, &mi, ap) != 3) return t;
  h = h % 12 + (ap[0] == 'P' ? 12 : 0);
  char b[8]; snprintf(b, sizeof b, "%02d:%02d", h, mi);
  return b;
}

static std::string rangeEn(const std::string& a, const std::string& b) {
  return a + "–" + b + " ft (" + m1(atof(a.c_str())) + "–" + m1(atof(b.c_str())) + " m)";
}

std::vector<std::string> extractFacts(const std::string& text, bool en) {
  struct P { std::regex re; std::string (*zh)(const std::smatch&); std::string (*en)(const std::smatch&); };
  static const std::vector<P> pats = {
    {std::regex(R"(breaking waves of (\d+) to (\d+) ft)"), [](const std::smatch& m) { return "破浪 " + range(m[1], m[2]); },
     [](const std::smatch& m) { return "Breakers " + rangeEn(m[1], m[2]); }},
    {std::regex(R"(occasional sets of (\d+) to (\d+) ft)"), [](const std::smatch& m) { return "偶有 " + range(m[1], m[2]) + "的大浪组"; },
     [](const std::smatch& m) { return "Occasional sets " + rangeEn(m[1], m[2]); }},
    {std::regex(R"([Uu]p to ([\d.]+) ft of inundation)"), [](const std::smatch& m) { return "低洼处可能积水深达 " + m1(atof(m[1].str().c_str())) + "米（" + m[1].str() + "英尺）"; },
     [](const std::smatch& m) { return "Flooding up to " + m[1].str() + " ft (" + m1(atof(m[1].str().c_str())) + " m)"; }},
    {std::regex(R"(([\d.]+) ft above normal \(([\d.]+) ft MLLW\) at ([\d:]+ [AP]M) (\w+) (\d+/\d+))"),
     [](const std::smatch& m) { return m[5].str() + " " + to24(m[3]) + " 高潮约 " + m1(atof(m[2].str().c_str())) + "米（比正常高 " + m1(atof(m[1].str().c_str())) + "米）"; },
     [](const std::smatch& m) { return "High tide ~" + m1(atof(m[2].str().c_str())) + " m (" + m1(atof(m[1].str().c_str())) + " m above normal) " + m[5].str() + " " + to24(m[3]); }},
    {std::regex(R"([Ss]eas (\d+) to (\d+) ft)"), [](const std::smatch& m) { return "海浪 " + range(m[1], m[2]); },
     [](const std::smatch& m) { return "Seas " + rangeEn(m[1], m[2]); }},
    {std::regex(R"([Ww]inds? (\d+) to (\d+) kt)"), [](const std::smatch& m) { return "风速 " + m[1].str() + "–" + m[2].str() + " 节"; },
     [](const std::smatch& m) { return "Winds " + m[1].str() + "–" + m[2].str() + " kt"; }},
  };
  std::vector<std::string> out;
  for (auto& p : pats)
    for (auto it = std::sregex_iterator(text.begin(), text.end(), p.re); it != std::sregex_iterator(); ++it) out.push_back(en ? p.en(*it) : p.zh(*it));
  return out;
}

std::vector<std::string> keywordChips(const std::string& text) {
  static const char* K[][2] = {
    {"sneaker wave", "疯狗浪"}, {"rip current", "离岸流"}, {"over seawalls", "浪越过防波堤"}, {"road closure", "道路封闭"},
    {"jetties", "勿上防波堤"}, {"turn your back on the ocean", "勿背对大海"}, {"remain out of the water", "勿下水"},
  };
  std::string l = unwrap(text);
  std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c) { return (char)std::tolower(c); });
  std::vector<std::string> out;
  for (auto& k : K) if (l.find(k[0]) != std::string::npos) out.push_back(k[1]);
  return out;
}

std::string englishText(const std::string& description, const std::string& instruction) {
  std::string o;
  auto secs = splitSections(description);
  if (secs.empty()) o = unwrap(description);
  for (auto& s : secs) o += (o.empty() ? "" : "\n") + std::string("* ") + s.name + "..." + s.text;
  // instruction: keep paragraphs (blank-line separated)
  std::string para, line;
  std::istringstream in(instruction);
  auto flush = [&]() { if (!para.empty()) { o += "\n" + unwrap(para); para.clear(); } };
  while (std::getline(in, line)) {
    if (unwrap(line).empty()) flush();
    else para += line + "\n";
  }
  flush();
  return o;
}

std::string hashKey(const std::string& s) {
  uint32_t h = 2166136261u;
  for (unsigned char c : s) { h ^= c; h *= 16777619u; }
  char b[12]; snprintf(b, sizeof b, "%08x", h);
  return b;
}

}  // namespace core
