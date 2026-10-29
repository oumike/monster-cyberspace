#include "ui/eink.h"

namespace rc {

bool rgb565Dark(uint16_t c) {
  const uint32_t r = ((c >> 11) & 0x1F) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
  return (r * 77 + g * 150 + b * 29) >> 8 < 128;
}

void packMono(const uint16_t* frame, int w, int y0, int y1, uint8_t* mono) {
  const int stride = (w + 7) / 8;
  for (int y = y0; y < y1; ++y) {
    const uint16_t* src = frame + y * w;
    uint8_t* dst = mono + y * stride;
    for (int bx = 0; bx < stride; ++bx) {
      uint8_t byte = 0xFF;
      for (int bit = 0; bit < 8; ++bit) {
        const int x = bx * 8 + bit;
        if (x < w && rgb565Dark(src[x])) byte &= uint8_t(~(0x80 >> bit));
      }
      dst[bx] = byte;
    }
  }
}

void EinkPolicy::changed(uint32_t now, bool full) {
  if (!pending_) pendingSince_ = now;
  pending_ = true;
  fullPending_ = fullPending_ || full;
}

EinkRefresh EinkPolicy::due(uint32_t now, uint32_t lastInputMs, bool bigChange) const {
  if (!pending_) return EinkRefresh::None;
  if (refreshed_ && now - lastMs_ < kEinkMinGapMs) return EinkRefresh::None;
  if (fullPending_) return EinkRefresh::Full;
  if (now - lastInputMs < kEinkTypingQuietMs && now - pendingSince_ < kEinkTypingMaxMs)
    return EinkRefresh::None;
  if (partials_ >= kEinkFullEvery || (bigChange && partials_ >= kEinkFullOnBigChange))
    return EinkRefresh::Full;
  return EinkRefresh::Partial;
}

void EinkPolicy::sent(EinkRefresh r, uint32_t now) {
  if (r == EinkRefresh::None) return;
  refreshed_ = true;
  lastMs_ = now;
  pending_ = false;
  if (r == EinkRefresh::Full) {
    fullPending_ = false;
    partials_ = 0;
  } else {
    ++partials_;
  }
}

}  // namespace rc
