#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

enum class OskPage : uint8_t { Letters, Symbols, More };
enum class OskKind : uint8_t { Char, Shift, Page, Space, Backspace, Enter, Hide, Emoji };

struct OskKey {
  OskKind kind;
  char32_t ch = 0;  // Char: the lower-case/base character
  std::u32string label;
  OskPage to = OskPage::Letters;  // Page: the page it opens
  int col = 0, row = 0, w = 0, h = 0;  // cells within the keyboard grid
};

struct OskOutput {
  KeyEvent key;       // key.action None: nothing for the screen; Emoji: the tray
  bool hide = false;
};

constexpr uint32_t kShiftLockMs = 400;  // second Shift tap within this locks caps

// The on-screen keyboard: three pages of keys laid out in a grid, driven by
// taps resolved to a key index. It emits KeyEvents for the focused screen and
// keeps its own Shift/caps state. Board-agnostic: it never touches hardware.
class Osk {
 public:
  static int keyRows(TextSize s);  // 2 Comfortable (32 px), 3 Compact (36 px)
  static int rows(TextSize s);     // 4 * keyRows(s)
  void layout(int cols, TextSize s);  // rebuilds keys() for the current page
  void reset();                       // Letters page, Shift off (each time it is shown)
  const std::vector<OskKey>& keys() const { return keys_; }
  OskPage page() const { return page_; }
  bool shifted() const { return shifted_; }
  bool capsLock() const { return caps_; }
  OskOutput press(int i, uint32_t nowMs);
  // Fills g (a rows()-row grid) and registers one Osk target per key.
  void render(Grid& g, const Theme& t) const;

 private:
  std::vector<OskKey> keys_;
  OskPage page_ = OskPage::Letters;
  bool shifted_ = false, caps_ = false;
  uint32_t lastShiftMs_ = 0;
  int cols_ = 0;
  TextSize size_ = TextSize::Comfortable;
};

}  // namespace rc
