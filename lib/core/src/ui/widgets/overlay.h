#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/theme.h"
#include "ui/widgets/list_view.h"

namespace rc {

// Where a menu or confirm box sits on the screen. Each one copies the layout a
// screen used before the Overlay widget existed.
enum class OverlayPlace : uint8_t { BottomRight, Centre, Bottom };

// The one menu/confirm box: a titled, wrapping ListView that a screen drops
// over its content. Up/Down move; Select/Right pick; Back/Left close. An
// Activate aimed at the menu moves to a row then applies its follow-up key, so
// a tap can pick without a phantom Select on an out-of-range row.
class Overlay {
 public:
  enum class Result : uint8_t { None, Picked, Closed };

  // A menu of `items`, with `selected` highlighted.
  void open(std::u32string title, std::vector<ListItem> items, int selected = 0);
  // A two-row confirm: "Cancel" (selected) and `action` in ItemTone::Error.
  // With confirmButtons() on, drawn as two buttons side by side at the bottom
  // instead, wherever it was placed.
  void confirm(std::u32string title, std::u32string action);
  // A menu that confirmButtons() draws as buttons, two a row at the bottom
  // under its title (when they fit); otherwise the usual list box.
  void openButtons(std::u32string title, std::vector<ListItem> items);
  Overlay() = default;
  Overlay(const Overlay&) = delete;
  Overlay& operator=(const Overlay&) = delete;
  ~Overlay() { close(); }
  void close();
  // True while any Overlay is open.
  static bool anyOpen();
  // Touch-only boards: confirms and button menus are buttons. Off by default.
  static void setConfirmButtons(bool on);
  static bool confirmButtons();
  bool isOpen() const { return open_; }
  int selected() const { return list_.selected(); }
  const std::u32string& title() const { return title_; }

  // Up/Down move and wrap; Select/Right pick (and close); Back/Left close;
  // an Activate with target Menu selects its index (if in range) then applies
  // `then`. Any other key leaves the box open and returns None.
  Result key(const KeyEvent& k);

  // Within rows [top, bottom) and columns [0, g.cols() - rightReserve).
  void render(Grid& g, int top, int bottom, int rightReserve, OverlayPlace p,
              const Theme& t) const;

 private:
  // openButtons() menus on touch-only boards; false when they don't fit.
  bool renderButtonMenu(Grid& g, int top, int bottom, int rightReserve, const Theme& t) const;

  ListView list_;
  std::u32string title_;
  bool open_ = false;
  bool confirm_ = false;  // opened by confirm()
  bool buttons_ = false;  // opened by confirm() or openButtons()
};

}  // namespace rc
