#pragma once

#include <lvgl.h>

#include "input/gestures.h"
#include "input/key_event.h"

namespace lvui {

// Touch on a scrolling log (rc::LogView, rc::TextView) as rc::Gestures and
// rc::TouchResolver turn it into keys on a non-wrapping, hold-to-act list: a
// tap emits {Activate, item, list, then Select} (touch only: nothing), a long
// press then Palette (touch only: then Select); a drag steps one Up/Down per
// cell of travel (finger up: Down), a swipe a screen less one line.
class RowGestures {
 public:
  // What lies under the finger: the item at a point, -1 for none (a divider,
  // past the end), and how many rows hold an item (TapMap::visibleRows).
  class Rows {
   public:
    virtual int itemAt(lv_point_t p) const = 0;
    virtual int shownItems() const = 0;

   protected:
    ~Rows() = default;
  };

  // obj takes every press on it (no LVGL scrolling, no scroll chaining).
  void attach(lv_obj_t* obj, Rows* rows, rc::ListId list);

 private:
  static void eventCb(lv_event_t* e);
  void feed(bool down);
  void act(const rc::Gesture& g);

  lv_obj_t* obj_ = nullptr;
  Rows* rows_ = nullptr;
  rc::ListId list_ = rc::ListId::Log;
  rc::Gestures gestures_;
  int carry_ = 0;
};

}  // namespace lvui
