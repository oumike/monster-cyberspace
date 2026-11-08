#include "lvui/views/row_gestures.h"

#include <cstdlib>

#include "lvui/input.h"
#include "lvui/views/view_util.h"

namespace lvui {

void RowGestures::attach(lv_obj_t* obj, Rows* rows, rc::ListId list) {
  obj_ = obj;
  rows_ = rows;
  list_ = list;
  // As ui_task's own Gestures: the edge zones are an eighth of the panel each.
  gestures_.setWidth(lv_display_get_horizontal_resolution(lv_obj_get_display(obj)));
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_add_event_cb(obj, eventCb, LV_EVENT_PRESSED, this);
  lv_obj_add_event_cb(obj, eventCb, LV_EVENT_PRESSING, this);
  lv_obj_add_event_cb(obj, eventCb, LV_EVENT_RELEASED, this);
  lv_obj_add_event_cb(obj, eventCb, LV_EVENT_PRESS_LOST, this);
}

void RowGestures::eventCb(lv_event_t* e) {
  auto* self = static_cast<RowGestures*>(lv_event_get_user_data(e));
  const lv_event_code_t code = lv_event_get_code(e);
  self->feed(code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING);
}

void RowGestures::feed(bool down) {
  lv_indev_t* in = lv_indev_active();
  if (!in) return;
  lv_point_t p;
  lv_indev_get_point(in, &p);
  rc::Gesture g;
  if (gestures_.feed(down, static_cast<int16_t>(p.x), static_cast<int16_t>(p.y), lv_tick_get(), g)) act(g);
}

void RowGestures::act(const rc::Gesture& g) {
  const int item = rows_->itemAt({g.x, g.y});
  auto steps = [&](int n, bool towardLater) {
    rc::KeyEvent e;
    e.action = towardLater ? rc::Action::Down : rc::Action::Up;
    for (int i = 0; i < n; ++i) emit(e);
  };
  switch (g.kind) {
    case rc::GestureKind::Tap:
    case rc::GestureKind::LongPress: {
      if (item < 0) return;
      const bool hold = g.kind == rc::GestureKind::LongPress;
      if (touchOnly()) {
        // Taps scroll safely past; a long press acts on the row.
        if (hold) emit(rowKey(list_, item, false));
        return;
      }
      emit(rowKey(list_, item, hold));
      return;
    }
    case rc::GestureKind::Drag: {
      if (item < 0) {
        carry_ = 0;
        return;
      }
      const int h = cellOf(obj_).h;
      if (h <= 0) return;
      carry_ += g.dy;
      const int whole = carry_ / h;
      carry_ -= whole * h;
      if (whole != 0) steps(std::abs(whole), whole < 0);
      return;
    }
    case rc::GestureKind::Swipe:
      if (item < 0 || (g.dir != rc::SwipeDir::Up && g.dir != rc::SwipeDir::Down)) return;
      steps(rows_->shownItems() > 1 ? rows_->shownItems() - 1 : 1, g.dir == rc::SwipeDir::Up);
      return;
    default:
      // Edge swipes are the app's (input/gestures in ui_task); Hold does nothing.
      return;
  }
}

}  // namespace lvui
