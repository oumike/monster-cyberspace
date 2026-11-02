#include "ui/haptics.h"

namespace rc {

bool tapBuzz(const Gesture& g, const TouchOutput& out) {
  return g.kind == GestureKind::Tap && (!out.keys.empty() || out.oskKey >= 0);
}

}  // namespace rc
