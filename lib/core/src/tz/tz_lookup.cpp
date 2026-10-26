#include "tz/tz_lookup.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace rc {
namespace {

constexpr size_t kHeaderSize = 36;
constexpr size_t kZoneSize = 16;
constexpr size_t kRingHeaderSize = 24;
constexpr int kGridW = 360;
constexpr int kGridH = 180;
constexpr size_t kGridBytes = size_t(kGridW) * kGridH * 4;
constexpr uint32_t kEmptyCell = 0xFFFFFFFFu;
constexpr uint32_t kDirectCell = 0x80000000u;
constexpr double kTrackKm = 10.0;
constexpr int64_t kMaxLon = 180000000;  // µdeg
constexpr int64_t kMaxLat = 90000000;

// Bounds-checked little-endian reads; the blob's fields are unaligned.
struct Reader {
  const uint8_t* blob;
  size_t size;

  bool has(size_t off, size_t n) const { return off <= size && n <= size - off; }
  bool u16(size_t off, uint16_t& v) const {
    if (!has(off, 2)) return false;
    v = uint16_t(blob[off] | blob[off + 1] << 8);
    return true;
  }
  bool u32(size_t off, uint32_t& v) const {
    if (!has(off, 4)) return false;
    v = uint32_t(blob[off]) | uint32_t(blob[off + 1]) << 8 | uint32_t(blob[off + 2]) << 16 |
        uint32_t(blob[off + 3]) << 24;
    return true;
  }
  bool i32(size_t off, int32_t& v) const {
    uint32_t u;
    if (!u32(off, u)) return false;
    memcpy(&v, &u, 4);
    return true;
  }
  // A NUL-terminated string that ends inside the blob.
  bool str(size_t off, std::string& out) const {
    if (off >= size) return false;
    const void* end = memchr(blob + off, 0, size - off);
    if (!end) return false;
    out.assign(reinterpret_cast<const char*>(blob + off),
               static_cast<const uint8_t*>(end) - (blob + off));
    return true;
  }
  bool varint(size_t& off, size_t end, int64_t& v) const {
    uint64_t z = 0;
    for (int shift = 0; shift < 64; shift += 7) {
      if (off >= end) return false;
      uint8_t b = blob[off++];
      z |= uint64_t(b & 0x7F) << shift;
      if (!(b & 0x80)) {
        v = int64_t(z >> 1) ^ -int64_t(z & 1);
        return true;
      }
    }
    return false;
  }
};

struct Header {
  uint16_t zoneCount;
  uint32_t zonesOff;
  uint32_t gridOff;
};

bool readHeader(const Reader& r, Header& h) {
  if (!r.blob || !r.has(0, kHeaderSize) || memcmp(r.blob, "RCTZ", 4) != 0) return false;
  uint16_t version;
  if (!r.u16(4, version) || version != 1) return false;
  if (!r.u16(6, h.zoneCount) || !r.u32(8, h.zonesOff) || !r.u32(12, h.gridOff)) return false;
  return r.has(h.zonesOff, size_t(h.zoneCount) * kZoneSize) && r.has(h.gridOff, kGridBytes);
}

// Even-odd crossing count of one ring for the point (x, y) in µdeg; false
// on an out-of-range read.
bool ringCrossings(const Reader& r, size_t& off, int64_t x, int64_t y, bool& odd) {
  int32_t minLon, minLat, maxLon, maxLat;
  uint32_t count, bytes;
  if (!r.i32(off, minLon) || !r.i32(off + 4, minLat) || !r.i32(off + 8, maxLon) ||
      !r.i32(off + 12, maxLat) || !r.u32(off + 16, count) || !r.u32(off + 20, bytes))
    return false;
  size_t data = off + kRingHeaderSize;
  if (!r.has(data, bytes)) return false;
  off = data + bytes;
  // Outside the box, the ring has no crossing to the right of the point.
  if (x < minLon || x > maxLon || y < minLat || y > maxLat || count < 3) return true;
  size_t p = data;
  const size_t end = data + bytes;
  int64_t x0 = 0, y0 = 0, xi = 0, yi = 0;
  for (uint32_t i = 0; i <= count; ++i) {
    int64_t xj, yj;
    if (i < count) {
      int64_t dx, dy;
      if (!r.varint(p, end, dx) || !r.varint(p, end, dy)) return false;
      // Corrupt deltas would overflow the int64 crossing maths below.
      if (dx < -kMaxLon * 2 || dx > kMaxLon * 2 || dy < -kMaxLat * 2 || dy > kMaxLat * 2)
        return false;
      xj = xi + dx;
      yj = yi + dy;
      if (xj < -kMaxLon || xj > kMaxLon || yj < -kMaxLat || yj > kMaxLat) return false;
      if (i == 0) {
        x0 = xi = xj;
        y0 = yi = yj;
        continue;
      }
    } else {
      xj = x0;
      yj = y0;
    }
    if ((yi > y) != (yj > y) && x < xi + (xj - xi) * (y - yi) / (yj - yi)) odd = !odd;
    xi = xj;
    yi = yj;
  }
  return true;
}

bool zoneContains(const Reader& r, const Header& h, uint16_t zone, int64_t x, int64_t y) {
  if (zone >= h.zoneCount) return false;
  const size_t rec = h.zonesOff + size_t(zone) * kZoneSize;
  uint32_t ringsOff, ringCount;
  if (!r.u32(rec + 8, ringsOff) || !r.u32(rec + 12, ringCount)) return false;
  size_t off = ringsOff;
  bool odd = false;
  for (uint32_t i = 0; i < ringCount; ++i) {
    if (!ringCrossings(r, off, x, y, odd)) return false;
  }
  return odd;
}

bool zoneStrings(const Reader& r, const Header& h, uint16_t zone, TzZone& out) {
  const size_t rec = h.zonesOff + size_t(zone) * kZoneSize;
  uint32_t nameOff, posixOff;
  return r.u32(rec, nameOff) && r.u32(rec + 4, posixOff) && r.str(nameOff, out.name) &&
         r.str(posixOff, out.posix);
}

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

double clampDouble(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace

const char* tzModeKey(TzMode mode) { return mode == TzMode::Automatic ? "auto" : "manual"; }

bool parseTzMode(std::string_view key, TzMode& out) {
  if (key == "auto") {
    out = TzMode::Automatic;
    return true;
  }
  if (key == "manual") {
    out = TzMode::Manual;
    return true;
  }
  return false;
}

bool tzBlobValid(const uint8_t* blob, size_t size) {
  Reader r{blob, size};
  Header h;
  if (!readHeader(r, h)) return false;
  for (uint16_t i = 0; i < h.zoneCount; ++i) {
    const size_t rec = h.zonesOff + size_t(i) * kZoneSize;
    uint32_t ringsOff, ringCount;
    TzZone z;
    if (!zoneStrings(r, h, i, z) || !r.u32(rec + 8, ringsOff) || !r.u32(rec + 12, ringCount))
      return false;
    if (ringCount > 0 && !r.has(ringsOff, kRingHeaderSize)) return false;
  }
  return true;
}

TzZone oceanZone(double lon) {
  if (!std::isfinite(lon)) lon = 0;
  const int n = clampInt(int(std::floor(clampDouble(lon, -180, 180) / 15.0 + 0.5)), -12, 12);
  if (n == 0) return {"Etc/GMT", "GMT0"};
  const int a = n < 0 ? -n : n;
  char name[16], posix[16];
  // East of Greenwich is ahead of UTC, and both names invert the sign.
  snprintf(name, sizeof name, "Etc/GMT%c%d", n > 0 ? '-' : '+', a);
  snprintf(posix, sizeof posix, "<%c%02d>%s%d", n > 0 ? '+' : '-', a, n > 0 ? "-" : "", a);
  return {name, posix};
}

bool tzLookup(const uint8_t* blob, size_t size, double lat, double lon, TzZone& out) {
  Reader r{blob, size};
  Header h;
  if (!std::isfinite(lat) || !std::isfinite(lon) || !readHeader(r, h)) {
    out = oceanZone(lon);
    return false;
  }
  lat = clampDouble(lat, -90, 90);
  lon = clampDouble(lon, -180, 180);
  const int row = clampInt(int(std::floor(lat)), -90, 89) + 90;
  const int col = clampInt(int(std::floor(lon)), -180, 179) + 180;
  uint32_t cell = kEmptyCell;
  r.u32(h.gridOff + (size_t(row) * kGridW + col) * 4, cell);
  const int64_t x = std::llround(lon * 1e6);
  const int64_t y = std::llround(lat * 1e6);

  if (cell != kEmptyCell) {
    if (cell & kDirectCell) {
      const uint32_t zone = cell & ~kDirectCell;
      if (zone < h.zoneCount && zoneStrings(r, h, uint16_t(zone), out)) return true;
    } else {
      uint16_t n;
      if (r.u16(cell, n)) {
        for (uint16_t i = 0; i < n; ++i) {
          uint16_t zone;
          if (!r.u16(size_t(cell) + 2 + size_t(i) * 2, zone)) break;
          if (zoneContains(r, h, zone, x, y) && zoneStrings(r, h, zone, out)) return true;
        }
      }
    }
  }
  out = oceanZone(lon);
  return false;
}

double distanceKm(double lat1, double lon1, double lat2, double lon2) {
  constexpr double kRad = M_PI / 180.0;
  const double dLat = (lat2 - lat1) * kRad;
  const double dLon = (lon2 - lon1) * kRad;
  const double s = std::sin(dLat / 2);
  const double t = std::sin(dLon / 2);
  const double a = s * s + std::cos(lat1 * kRad) * std::cos(lat2 * kRad) * t * t;
  return 2 * 6371.0 * std::asin(std::sqrt(clampDouble(a, 0, 1)));
}

bool TzTracker::update(const uint8_t* blob, size_t size, double lat, double lon, TzZone& out) {
  if (has_ && distanceKm(lat_, lon_, lat, lon) <= kTrackKm) return false;
  tzLookup(blob, size, lat, lon, out);
  has_ = true;
  lat_ = lat;
  lon_ = lon;
  return true;
}

void TzTracker::reset() { has_ = false; }

const std::string& effectiveTz(TzMode mode, const std::string& manualTz,
                               const std::string& autoPosix) {
  return mode == TzMode::Automatic && !autoPosix.empty() ? autoPosix : manualTz;
}

}  // namespace rc
