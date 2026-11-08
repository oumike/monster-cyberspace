#pragma once

#include <lvgl.h>

#include "input/gestures.h"
#include "input/key_event.h"

namespace lvui {

// Touch on a scrolling log (rc::LogView, rc::TextView) as rc::Gestures and
// rc::TouchResolver turn it into keys on a non-wrapping, hold-to-act list: a
// tap emits {Activate, item, list, then Select} (touch only: nothing), a long
// press then Palette (touch only: then Select); a drag steps one Up/Down per
// cell of travel (finger up: Down), a swipe a screen less one line. A
// wrapping list (rc::EntryList, TapMap::setList wraps) steps as the
// resolver's stepList does: one {Activate, item, list, then None} to the
// clamped item, nothing when it doesn't move.
class RowGestures {
 public:
  // What lies under the finger: the item at a point, -1 for none (a divider,
  // past the end), and how many rows hold an item (TapMap::visibleRows).
  class Rows {
   public:
    virtual int itemAt(lv_point_t p) const = 0;
    virtual int shownItems() const = 0;
    // A row target lies under p even where no item does (a button grid's
    // background, which takes drags); a tap there emits row index -1.
    virtual bool onRows(lv_point_t p) const { return itemAt(p) >= 0; }
    // A tap or long press on nothing at all (not onRows).
    virtual void missed(bool hold) { (void)hold; }
    // A wrapping list's selected item and item count (TapMap::ListInfo).
    virtual int selectedItem() const { return -1; }
    virtual int itemCount() const { return 0; }

   protected:
    ~Rows() = default;
  };

  // obj takes every press on it (no LVGL scrolling, no scroll chaining).
  // holdToAct false: a list that isn't a scrolling log (rc::ListInfo), where
  // touch-only boards tap rows as the rest do. wraps: drags and swipes step
  // a wrapping list (see above).
  void attach(lv_obj_t* obj, Rows* rows, rc::ListId list, bool holdToAct = true, bool wraps = false);
  // The list later keys name, after attach.
  void setList(rc::ListId list) { list_ = list; }

 private:
  static void eventCb(lv_event_t* e);
  void feed(bool down);
  void act(const rc::Gesture& g);

  lv_obj_t* obj_ = nullptr;
  Rows* rows_ = nullptr;
  rc::ListId list_ = rc::ListId::Log;
  bool holdToAct_ = true;
  bool wraps_ = false;
  rc::Gestures gestures_;
  int carry_ = 0;
};

}  // namespace lvui
