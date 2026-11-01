#pragma once

#include "ui/text_width.h"

namespace rc {

// A build without emoji bitmaps (RC_FEAT_EMOJI 0) still lays a wide emoji out
// in two cells; it draws the placeholder in the left one and a blank in the
// tail. '?' because the cell fonts have neither U+25A1 nor U+25AF.
constexpr char32_t emojiPlaceholder() { return U'?'; }

// What cell cp draws when emoji are off.
inline char32_t emojiStandIn(char32_t cp) {
  if (emojiIndex(cp) >= 0) return emojiPlaceholder();
  if (isWideTail(cp)) return U' ';
  return cp;
}

}  // namespace rc
