#pragma once

#include <cstdint>

namespace rc {

// RGB565 to the 1-bit frame an e-paper panel takes: 1 is white, MSB first,
// (w + 7) / 8 bytes a row. A pixel is black below half luminance.
bool rgb565Dark(uint16_t c);
// Packs rows [y0, y1) of a w-wide RGB565 frame into the same rows of mono.
void packMono(const uint16_t* frame, int w, int y0, int y1, uint8_t* mono);

enum class EinkRefresh : uint8_t { None, Partial, Full };

// When an e-paper panel refreshes, after platy-aprs's display_eink.cpp: no
// closer together than kEinkMinGapMs; while keys are coming, held until
// kEinkTypingQuietMs of quiet, but never past kEinkTypingMaxMs; a full refresh
// (which clears ghosting) every kEinkFullEvery partials, or for a big change
// (a new screen) once kEinkFullOnBigChange partials have built up.
constexpr uint32_t kEinkMinGapMs = 250, kEinkTypingQuietMs = 150, kEinkTypingMaxMs = 1200;
constexpr uint8_t kEinkFullEvery = 10, kEinkFullOnBigChange = 3;

class EinkPolicy {
 public:
  // The frame changed (full: the panel needs a full refresh, as after waking).
  void changed(uint32_t now, bool full = false);
  bool pending() const { return pending_; }
  // What to send now. lastInputMs is the last key press; bigChange whether
  // much of the frame differs from what the panel shows.
  EinkRefresh due(uint32_t now, uint32_t lastInputMs, bool bigChange) const;
  void sent(EinkRefresh r, uint32_t now);
  // The changed frame matched what the panel already shows.
  void drop() { pending_ = false; }

 private:
  bool pending_ = false, fullPending_ = true, refreshed_ = false;
  uint32_t pendingSince_ = 0, lastMs_ = 0;
  uint8_t partials_ = 0;
};

}  // namespace rc
