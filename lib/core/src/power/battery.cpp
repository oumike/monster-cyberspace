#include "power/battery.h"

namespace rc {

int batteryPercentFromVolts(float v) {
  if (v < 2.5f) return -1;  // no battery reading
  int pct = (int)((v - 3.3f) / (4.15f - 3.3f) * 100.0f + 0.5f);
  return pct < 0 ? 0 : pct > 100 ? 100 : pct;
}

}  // namespace rc
