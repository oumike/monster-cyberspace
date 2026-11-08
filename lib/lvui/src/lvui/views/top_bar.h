#pragma once

#include <lvgl.h>

#include <string>

#include "lvui/views/view_util.h"
#include "ui/widgets/status_bar.h"

namespace lvui {

// What rc::renderStatusBar takes.
struct TopBarData {
  std::u32string title;
  int count = -1;  // < 0: hidden
  char32_t conn = U'\u25CF';
  int wifiBars = 0;
  int batteryPct = -1;  // < 0: hidden
  std::string clock;
  rc::StatusTouch touch;

  bool operator==(const TopBarData& o) const {
    return title == o.title && count == o.count && conn == o.conn && wifiBars == o.wifiBars &&
           batteryPct == o.batteryPct && clock == o.clock && touch.on == o.touch.on &&
           touch.back == o.touch.back && touch.menu == o.touch.menu;
  }
};

// The status bar: one Role::Bar row, " title count" at the left and
// "marker bars battery% clock " at the right, as rc::renderStatusBar lays it
// out. With touch, ‹ at the left (tap Back, hold OpenHome, 4 cells) and ⋯
// after the clock (Palette, the last 4 cells).
class TopBar : public Tappable {
 public:
  TopBar() = default;
  TopBar(const TopBar&) = delete;
  TopBar& operator=(const TopBar&) = delete;

  void create(lv_obj_t* parent);
  void sync(const TopBarData& d);

  lv_obj_t* obj() const { return bar_; }
  lv_obj_t* title() const { return title_; }
  lv_obj_t* right() const { return right_; }
  lv_obj_t* back() const { return backHit_; }
  lv_obj_t* menu() const { return menuHit_; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  lv_obj_t *bar_ = nullptr, *glyph_ = nullptr, *title_ = nullptr, *right_ = nullptr;
  lv_obj_t *backHit_ = nullptr, *menuHit_ = nullptr;
  TopBarData last_;
  int lastW_ = -1;
};

}  // namespace lvui
