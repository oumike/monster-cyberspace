#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// Offline time-zone lookup from a position, over a blob built by
// scripts/tzgen.py. The blob is little-endian with absolute offsets:
//   header (36 bytes): "RCTZ", u16 version = 1, u16 zoneCount, u32 zonesOff,
//     u32 gridOff, char boundaryRelease[8], char tzdataVersion[8],
//     u32 toleranceMicroDeg;
//   zone records (16 bytes each, at zonesOff): u32 nameOff, u32 posixOff,
//     u32 ringsOff, u32 ringCount;
//   rings: i32 minLon, minLat, maxLon, maxLat (µdeg), u32 pointCount,
//     u32 dataBytes, then pointCount zigzag LEB128 (dlon, dlat) deltas in µdeg
//     from the previous point (the first from (0, 0)); rings close implicitly,
//     and outer rings and holes are combined by the even-odd rule;
//   grid (at gridOff): 360 × 180 u32 cells for 1° squares, indexed
//     (floor(lat) + 90) * 360 + floor(lon) + 180: 0xFFFFFFFF no zone,
//     0x80000000 | zone for a cell wholly inside one zone, otherwise the
//     offset of {u16 n, u16 zone[n]}.
namespace rc {

enum class TzMode : uint8_t { Automatic, Manual };

const char* tzModeKey(TzMode mode);  // "auto" | "manual"
bool parseTzMode(std::string_view key, TzMode& out);

struct TzZone {
  std::string name;
  std::string posix;
};

// The magic, the version, and every header and zone-record offset in range.
bool tzBlobValid(const uint8_t* blob, size_t size);

// True when the position is inside a zone polygon. Otherwise false, with
// out = oceanZone(lon); that includes an invalid blob.
bool tzLookup(const uint8_t* blob, size_t size, double lat, double lon, TzZone& out);

// The nautical zone: n = clamp(round(lon / 15), -12, 12), halves rounded up;
// "Etc/GMT-n" for n > 0 (east), "Etc/GMT+n" for n < 0, "Etc/GMT" for 0.
TzZone oceanZone(double lon);

// Great-circle distance (haversine, R = 6371 km).
double distanceKm(double lat1, double lon1, double lat2, double lon2);

// Looks the zone up again only after moving more than 10 km.
class TzTracker {
 public:
  // True when it looked the zone up: on the first call, or more than 10 km
  // from the last lookup. Otherwise out is left unchanged.
  bool update(const uint8_t* blob, size_t size, double lat, double lon, TzZone& out);
  void reset();

 private:
  bool has_ = false;
  double lat_ = 0;
  double lon_ = 0;
};

// Automatic with a known autoPosix gives autoPosix; otherwise manualTz.
const std::string& effectiveTz(TzMode mode, const std::string& manualTz,
                               const std::string& autoPosix);

}  // namespace rc
