#include "lvui/views/keyboard.h"

#include <algorithm>
#include <cstdlib>
#include <numeric>

#include "input/gestures.h"

#include "lvui/cell_font.h"
#include "lvui/input.h"
#include "lvui/styles.h"
#include "lvui/views/view_util.h"
#include "util/utf8.h"

namespace lvui {

namespace {

constexpr int kMaxUnits = 15;  // LV_BUTTONMATRIX_WIDTH_MASK

}  // namespace

void Keyboard::create(lv_obj_t* parent) {
  box_ = plainBox(parent);
  lv_obj_set_width(box_, lv_pct(100));
  matrix_ = lv_buttonmatrix_create(box_);
  lv_obj_remove_style_all(matrix_);
  lv_obj_remove_flag(matrix_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(matrix_, eventCb, LV_EVENT_PRESSED, this);
  lv_obj_add_event_cb(matrix_, eventCb, LV_EVENT_PRESSING, this);
  lv_obj_add_event_cb(matrix_, eventCb, LV_EVENT_PRESS_LOST, this);
  lv_obj_add_event_cb(matrix_, eventCb, LV_EVENT_VALUE_CHANGED, this);
  keys_ = -1;
  pressKey_ = -1;
}

int Keyboard::keyOf(uint32_t button) const {
  return button < buttonKey_.size() ? buttonKey_[button] : -1;
}

void Keyboard::eventCb(lv_event_t* e) {
  auto* self = static_cast<Keyboard*>(lv_event_get_user_data(e));
  if (!self->osk_) return;
  const lv_event_code_t code = lv_event_get_code(e);
  lv_point_t p{0, 0};
  if (lv_indev_t* in = lv_indev_active()) lv_indev_get_point(in, &p);
  if (code == LV_EVENT_PRESS_LOST) {
    self->pressKey_ = -1;
    return;
  }
  if (code == LV_EVENT_PRESSING) {
    // A finger that slides past the tap slop is a drag: it types nothing
    // (LVGL would type the key under the release).
    if (self->pressKey_ >= 0 &&
        (std::abs(p.x - self->pressAt_.x) > rc::kTapSlopPx || std::abs(p.y - self->pressAt_.y) > rc::kTapSlopPx))
      self->pressKey_ = -1;
    return;
  }
  const uint32_t button = lv_buttonmatrix_get_selected_button(self->matrix_);
  const int k = self->keyOf(button);
  if (code == LV_EVENT_PRESSED) {
    self->pressKey_ = k;
    self->pressAt_ = p;
    if (k < 0) return;
    // The light is the key the finger went down on, at once.
    self->osk_->touch(k, lv_tick_get());
    if (!self->on_[k]) {
      self->on_[k] = 1;
      lv_buttonmatrix_set_button_ctrl(self->matrix_, button, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    return;
  }
  const bool same = k >= 0 && k == self->pressKey_;
  self->pressKey_ = -1;
  if (!same) return;
  // The press may turn the page: the map is rebuilt at the next sync.
  const rc::OskOutput o = self->osk_->press(k, lv_tick_get());
  if (o.key.action != rc::Action::None) emit(o.key);
  if (o.hide && self->onHide_) self->onHide_();
}

// The Osk's rows as matrix rows: each key's width, and the gaps before and
// after a row (the letters' half-unit offset) as hidden spacers, in units of
// the row's greatest common width so every edge lands on Osk's cell.
void Keyboard::rebuild(const rc::Osk& osk, rc::TextSize ts) {
  const std::vector<rc::OskKey>& keys = osk.keys();
  const int n = static_cast<int>(keys.size());
  const Cell c = cellOf(box_);
  int left = keys.empty() ? 0 : keys[0].col, right = left;
  for (const rc::OskKey& k : keys) {
    left = std::min(left, k.col);
    right = std::max(right, k.col + k.w);
  }
  // The widest row sets the matrix's span (10 units).
  struct Seg {
    int w, key;
  };
  std::vector<std::vector<Seg>> rows;
  for (int i = 0; i < n; ++i) {
    const rc::OskKey& k = keys[i];
    const int r = k.row / 3;
    if (r >= static_cast<int>(rows.size())) rows.resize(r + 1);
    std::vector<Seg>& row = rows[r];
    int at = left;
    for (const Seg& s : row) at += s.w;
    if (k.col > at) row.push_back({k.col - at, -1});
    row.push_back({k.w, i});
  }
  for (std::vector<Seg>& row : rows) {
    int at = left;
    for (const Seg& s : row) at += s.w;
    if (at < right) row.push_back({right - at, -1});
  }

  labels_.clear();
  buttonKey_.clear();
  for (const rc::OskKey& k : keys) labels_.push_back(rc::u32ToUtf8(k.label));
  // Labels first so the pointers below stay put.
  static const char* const kSpacer = " ";
  static const char* const kRowEnd = "\n";
  static const char* const kEnd = "";
  map_.clear();
  std::vector<lv_buttonmatrix_ctrl_t> ctrl;
  for (size_t r = 0; r < rows.size(); ++r) {
    if (r) map_.push_back(kRowEnd);
    int g = 0;
    for (const Seg& s : rows[r]) g = std::gcd(g, s.w);
    if (g <= 0) g = 1;
    for (const Seg& s : rows[r]) {
      map_.push_back(s.key >= 0 ? labels_[s.key].c_str() : kSpacer);
      buttonKey_.push_back(s.key);
      int units = std::clamp(s.w / g, 1, kMaxUnits);
      lv_buttonmatrix_ctrl_t f = static_cast<lv_buttonmatrix_ctrl_t>(units);
      f = static_cast<lv_buttonmatrix_ctrl_t>(f | (s.key >= 0 ? LV_BUTTONMATRIX_CTRL_CLICK_TRIG | LV_BUTTONMATRIX_CTRL_NO_REPEAT
                                                              : LV_BUTTONMATRIX_CTRL_HIDDEN));
      ctrl.push_back(f);
    }
  }
  map_.push_back(kEnd);
  lv_buttonmatrix_set_map(matrix_, map_.data());
  lv_buttonmatrix_set_ctrl_map(matrix_, ctrl.data());
  on_.assign(n, 0);

  const int band = rc::Osk::keyRows(ts) * c.h;
  lv_obj_set_height(box_, rc::Osk::rows(ts) * c.h);
  place(matrix_, left * c.w, 0, (right - left) * c.w, static_cast<int>(rows.size()) * band);
}

void Keyboard::sync(rc::Osk& osk, rc::TextSize ts) {
  osk_ = &osk;
  const std::vector<rc::OskKey>& keys = osk.keys();
  const Cell c = cellOf(box_);
  const int n = static_cast<int>(keys.size());
  const int col0 = n ? keys[0].col : -1, w0 = n ? keys[0].w : -1;
  if (theme_ != themeSerial() || cw_ != c.w || ch_ != c.h) {
    theme_ = themeSerial();
    const rc::Theme& t = currentTheme();
    setBgColor(box_, t.bg);
    lv_obj_set_style_bg_opa(matrix_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(matrix_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(matrix_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(matrix_, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(matrix_, color(t.inputBg), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(matrix_, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix_, color(t.fg), LV_PART_ITEMS);
    lv_obj_set_style_border_color(matrix_, color(t.dim), LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix_, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(matrix_, 0, LV_PART_ITEMS);
    // Only the Osk's light (checked): LVGL's pressed key follows a sliding
    // finger, the light stays on the key it went down on.
    const lv_style_selector_t lit = LV_PART_ITEMS | LV_STATE_CHECKED;
    lv_obj_set_style_bg_color(matrix_, color(t.selectBg), lit);
    lv_obj_set_style_text_color(matrix_, color(t.selectFg), lit);
  }
  if (n != keys_ || osk.page() != page_ || col0 != col0_ || w0 != w0_ || ts != ts_ || c.w != cw_ || c.h != ch_) {
    keys_ = n;
    page_ = osk.page();
    col0_ = col0;
    w0_ = w0;
    ts_ = ts;
    cw_ = c.w;
    ch_ = c.h;
    rebuild(osk, ts);
  }
  const int lit = osk.lit(lv_tick_get());
  const bool shiftOn = osk.shifted() || osk.capsLock();
  for (uint32_t b = 0; b < buttonKey_.size(); ++b) {
    const int k = buttonKey_[b];
    if (k < 0) continue;
    const uint8_t want = (keys[k].kind == rc::OskKind::Shift && shiftOn) || k == lit;
    if (want == on_[k]) continue;
    on_[k] = want;
    if (want)
      lv_buttonmatrix_set_button_ctrl(matrix_, b, LV_BUTTONMATRIX_CTRL_CHECKED);
    else
      lv_buttonmatrix_clear_button_ctrl(matrix_, b, LV_BUTTONMATRIX_CTRL_CHECKED);
  }
}

}  // namespace lvui
