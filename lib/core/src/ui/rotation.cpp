#include "ui/rotation.h"

namespace rc {

const char* rotationLabel(uint8_t r) {
  switch (r) {
    case 1: return "Portrait";
    case 2: return "Landscape 180";
    case 3: return "Portrait 180";
    default: return "Landscape";
  }
}

uint8_t rotationFromIndex(int i) { return i >= 0 && i <= 3 ? uint8_t(i) : 0; }

bool rotationPortrait(uint8_t r) { return (r & 1) != 0; }

}  // namespace rc
