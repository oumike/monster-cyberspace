#include "power/battery.h"

namespace rc {

int batteryPercentFromVolts(float v) {
  if (v < 2.5f) return -1;  // no battery reading
  int pct = (int)((v - 3.3f) / (4.15f - 3.3f) * 100.0f + 0.5f);
  return pct < 0 ? 0 : pct > 100 ? 100 : pct;
}

int tanmatsuBatteryPercent(float v) {
  if (v < 2.5f) return -1;
  struct Point {
    float v;
    int pct;
  };
  static const Point kCurve[] = {{4.05f, 100}, {4.00f, 93}, {3.90f, 81}, {3.80f, 67}, {3.70f, 51},
                                 {3.60f, 36},  {3.50f, 22}, {3.40f, 10}, {3.30f, 0}};
  constexpr int kLast = sizeof kCurve / sizeof kCurve[0] - 1;
  if (v >= kCurve[0].v) return 100;
  if (v <= kCurve[kLast].v) return 0;
  for (int i = 0; i < kLast; ++i) {
    const Point& hi = kCurve[i];
    const Point& lo = kCurve[i + 1];
    if (v >= lo.v) return int(lo.pct + (hi.pct - lo.pct) * (v - lo.v) / (hi.v - lo.v) + 0.5f);
  }
  return 0;
}

bool tanmatsuVbatAgree(uint16_t a, uint16_t b, uint16_t& mv) {
  auto ok = [](uint16_t x) { return x >= 2304 && x <= 4848; };
  if (!ok(a) || !ok(b)) return false;
  const int diff = int(a) - int(b);
  if (diff < -40 || diff > 40) return false;
  mv = uint16_t((a + b) / 2);
  return true;
}

}  // namespace rc
