// NWS free-text handling, PRD §7.2 (port of prototype/alert_text.py). Only adds detail; never decides alert existence.
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace core {

std::string unwrap(const std::string& s);   // collapse hard-wrapped lines and runs of whitespace

struct Section { std::string name, text; };   // text is unwrapped
std::vector<Section> splitSections(const std::string& description);
std::string sectionText(const std::vector<Section>& s, const std::string& name);

struct Clause { std::string event; int ordinal; std::string text; };   // ordinal: 0 = none/first, 1 = second, 2 = third
std::vector<Clause> eventClauses(const std::string& what);            // empty when the text has no "For the <Event>,"

std::vector<std::string> extractFacts(const std::string& text, bool en = false);   // layer 3 (Chinese, or English for the landing page)
std::vector<std::string> keywordChips(const std::string& text);       // layer 4
std::string englishText(const std::string& description, const std::string& instruction);
std::string hashKey(const std::string& s);                            // FNV-1a hex

}  // namespace core
