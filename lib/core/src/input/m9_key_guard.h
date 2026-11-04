#pragma once

#include <cstdint>

namespace rc {

// Which key bytes from the M9's keyboard controller are new presses.
// Ported from wadamesh's M9KeyGuard.h (#625, #671, GPL-3.0).
//
// The controller (Elecrow's ThinkNode-M9-KB firmware) scans the matrix every
// 10 ms with no debounce and latches a key byte on every released-to-pressed
// edge. A contact that chatters as a key comes up is a second edge, so the same
// byte arrives again about one hold time after the first (100-250 ms on a slow
// press): a doubled letter. The controller can't say when a key went up, so the
// guard goes by time:
//  - The same character again inside a window is the same press. The window is
//    three fifths of the gap between two different characters (the typing
//    pace), 70-250 ms, and 250 ms until a pace is known. A chatter comes after
//    about one hold; a letter typed twice on purpose after a whole keystroke.
//  - Other keys (arrows, Backspace, Enter, function keys) keep 70 ms, so quick
//    taps down a list and a held Backspace's repeat get through.
class M9KeyGuard {
 public:
  static constexpr uint32_t kMinMs = 70;
  static constexpr uint32_t kMaxMs = 250;
  static constexpr uint32_t kPaceIdleMs = 1500;  // a longer gap is a pause

  // True when `key`, read at `nowMs`, is a new press. A dropped repeat doesn't
  // move the timing, so a run of bounces can't stretch the window.
  bool accept(uint8_t key, uint32_t nowMs);

 private:
  static bool isChar(uint8_t key) { return key >= 0x20 && key <= 0x7E; }
  uint32_t charWindow() const;

  uint8_t lastKey_ = 0;
  uint32_t lastMs_ = 0;
  uint32_t paceMs_ = 0;  // smoothed gap between two different characters
};

}  // namespace rc
