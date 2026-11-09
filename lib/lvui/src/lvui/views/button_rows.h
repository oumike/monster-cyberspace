#pragma once

#include <lvgl.h>

#include <string>
#include <vector>

#include "lvui/views/view_util.h"
#include "spell/spell.h"
#include "ui/widgets/spell_strip.h"
#include "ui/widgets/symbol_table.h"

namespace lvui {

// The one-row pickers. The palette and the canned picker are rc::Overlay
// menus (MenuOverlay draws them); the emoji tray is SymbolBox on the emoji set.

// rc::SpellStrip as a row of options on an inputBg bar, each " word " with a
// cell between them, as SpellStrip::render draws it: the selection filled
// (selectBg/selectFg) on colour themes and outlined in fg on outlined ones,
// the typed word (option 0) dim otherwise. A tap on option i emits
// {Spell, i}. Hidden while the strip is closed.
class SpellBar : public Tappable {
 public:
  SpellBar() = default;
  SpellBar(const SpellBar&) = delete;
  SpellBar& operator=(const SpellBar&) = delete;

  // A full-width, one-cell row at the parent's top left; the caller places it.
  void create(lv_obj_t* parent);
  // Drops the options that don't fit the bar's width (SpellStrip::fit).
  void sync(rc::SpellStrip& strip);

  lv_obj_t* obj() const { return bar_; }
  int shown() const { return shown_; }
  // Null until the bar first shows.
  lv_obj_t* option(int i) const { return opts_[i]; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  static constexpr int kMax = rc::spell::kMaxSuggest + 1;
  lv_obj_t* bar_ = nullptr;
  lv_obj_t* opts_[kMax] = {};
  // What the bar was last drawn from.
  std::u32string last_[kMax];
  int count_ = -1, shown_ = 0, sel_ = -1, cols_ = -1;
  unsigned theme_ = ~0u;
};

// rc::SymbolTable (symbols or the emoji tray) as SymbolTable::render draws
// it: a box in bg framed in drawerEdge with a barBg title row, centred above
// the parent's last cell row; the selection filled on colour themes and
// outlined in selectBg on outlined ones (Role::Selected). A tap on an item
// (its gap cell included, as the tap map registers it) emits
// {Activate, i, Menu}, the commands row index items().size(); a tap anywhere
// else Back. Hidden while the table is closed. Fills its parent.
class SymbolBox : public Tappable {
 public:
  SymbolBox() = default;
  SymbolBox(const SymbolBox&) = delete;
  SymbolBox& operator=(const SymbolBox&) = delete;

  void create(lv_obj_t* parent);
  void sync(const rc::SymbolTable& st);

  lv_obj_t* obj() const { return root_; }
  lv_obj_t* frame() const { return frame_; }
  lv_obj_t* title() const { return title_; }
  lv_obj_t* cell(int i) const { return cells_[i]; }
  lv_obj_t* commands() const { return commands_; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  void rebuild(const rc::SymbolTable& st, bool big);
  int hit(lv_point_t p) const;

  lv_obj_t *root_ = nullptr, *frame_ = nullptr, *titleBar_ = nullptr, *title_ = nullptr;
  lv_obj_t *commands_ = nullptr, *sbHost_ = nullptr;
  std::vector<lv_obj_t*> cells_;
  int n_ = 0;  // items in the set drawn
  ScrollBarView bar_;
  // What the box was last drawn from.
  rc::SymbolTable::Layout lay_;
  bool drawn_ = false, emoji_ = false, big_ = false;
  int sel_ = -1, cw_ = 0, ch_ = 0, gridCols_ = 0, gridRows_ = 0;
  std::u32string commandsText_;
  unsigned theme_ = ~0u;
};

}  // namespace lvui
