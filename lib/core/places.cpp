#include "model.h"

namespace core {

const char* kBayWaterStation = "9414750";

const std::vector<PlaceCfg>& places() {
  static const std::vector<PlaceCfg> P = {
    {"halfmoon", "半月湾", Profile::Coastal, 37.4636, -122.4286, "PZZ545", "9414131", "Pillar Point 站", "46012",
     37.4636, -122.4286, 37.48, -122.52, "潮汐 NOAA 9414131 · 浮标 46012 · 风浪/警示 NWS · 天气 Open-Meteo",
     "Half Moon Bay", "Pillar Point", "Tide NOAA 9414131 · Buoy 46012 · Wind/waves/alerts NWS · Weather Open-Meteo"},
    {"sf", "旧金山", Profile::Coastal, 37.7600, -122.5100, "PZZ545", "9414290", "San Francisco 站", "46026",
     37.7600, -122.5100, 37.76, -122.54, "潮汐 NOAA 9414290 · 浮标 46026 · 风浪/警示 NWS · 天气 Open-Meteo",
     "San Francisco", "San Francisco", "Tide NOAA 9414290 · Buoy 46026 · Wind/waves/alerts NWS · Weather Open-Meteo"},
    {"redwood", "红木城", Profile::Bay, 37.4852, -122.2364, "PZZ531", "9414523", "Redwood City 站", "",
     37.4852, -122.2364, 37.55, -122.22, "潮汐 NOAA 9414523 · 水温 阿拉米达站 · 风/警示 NWS · 天气 Open-Meteo",
     "Redwood City", "Redwood City", "Tide NOAA 9414523 · Water Alameda · Wind/alerts NWS · Weather Open-Meteo"},
    {"santacruz", "圣克鲁斯", Profile::Coastal, 36.9690, -122.0230, "PZZ535", "9413745", "Santa Cruz 站", "46236",
     36.9690, -122.0230, 36.93, -122.03, "潮汐 NOAA 9413745 · 浮标 46236 · 风浪/警示 NWS · 天气 Open-Meteo",
     "Santa Cruz", "Santa Cruz", "Tide NOAA 9413745 · Buoy 46236 · Wind/waves/alerts NWS · Weather Open-Meteo"},
  };
  return P;
}

}  // namespace core
