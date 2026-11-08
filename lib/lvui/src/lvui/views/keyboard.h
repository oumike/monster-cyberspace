#pragma once

#include <lvgl.h>

#include <string>
#include <vector>

#include "ui/grid.h"
#include "ui/widgets/osk.h"

namespace lvui {

// rc::Osk as an lv_buttonmatrix, as Osk::render draws it: Osk::rows(ts)
// cells tall and the parent's width, the keys at Osk's cells (each band
// keyRows rows of the font), in inputBg with fg labels and a 1 px dim
// outline, and in selectBg/selectFg while on (an active Shift, or the key
// touched until kOskFlashMs after). A finger on a key lights it
// (Osk::touch); a tap presses it (Osk::press) and emits the KeyEvent it
// makes, if any, then calls the hide callback when the key asks for that.
// Lay the Osk out (Osk::layout) for the parent's width in cells first.
class Keyboard {
 public:
  Keyboard() = default;
  Keyboard(const Keyboard&) = delete;
  Keyboard& operator=(const Keyboard&) = delete;

  void create(lv_obj_t* parent);
  // Rebuilds the keys when the Osk's page or layout changed; keeps a
  // pointer to osk for the taps.
  void sync(rc::Osk& osk, rc::TextSize ts);
  void setOnHide(void (*cb)()) { onHide_ = cb; }

  lv_obj_t* obj() const { return box_; }
  lv_obj_t* matrix() const { return matrix_; }
  // Osk key i's label and whether it is drawn on.
  const char* label(int i) const { return labels_[i].c_str(); }
  bool on(int i) const { return on_[i] != 0; }

 private:
  static void eventCb(lv_event_t* e);
  void rebuild(const rc::Osk& osk, rc::TextSize ts);
  int keyOf(uint32_t button) const;

  lv_obj_t *box_ = nullptr, *matrix_ = nullptr;
  rc::Osk* osk_ = nullptr;
  void (*onHide_)() = nullptr;
  // The map: the labels, a spacer " " and "\n" between rows, "" at the end.
  std::vector<std::string> labels_;
  std::vector<const char*> map_;
  std::vector<int> buttonKey_;  // button -> Osk key, -1 for a spacer
  std::vector<uint8_t> on_;      // per Osk key
  // What the keys were last built from.
  rc::OskPage page_ = rc::OskPage::Letters;
  int keys_ = -1, col0_ = -1, w0_ = -1, cw_ = 0, ch_ = 0;
  rc::TextSize ts_ = rc::TextSize::Comfortable;
  unsigned theme_ = ~0u;
};

}  // namespace lvui
