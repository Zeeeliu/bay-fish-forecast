// Sanity checks on a fetched body before it may replace the cached copy (HTTP 200 is not enough:
// a degraded server can answer 200 with nulls, an HTML error page or an error object).
#pragma once
#include <string>

namespace core {
bool validBody(const std::string& key, const std::string& body);   // key = "S3_sf", "S8_PZZ545", …
}
