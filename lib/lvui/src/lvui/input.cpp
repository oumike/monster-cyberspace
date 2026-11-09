#include "lvui/input.h"

#include <lvgl.h>

namespace lvui {

namespace {

bool (*readPointer)(int16_t&, int16_t&) = nullptr;
lv_indev_t* pointer = nullptr;
void (*keySink)(const rc::KeyEvent&) = nullptr;
bool onlyTouch = false;

void readCb(lv_indev_t*, lv_indev_data_t* data) {
  int16_t x = 0, y = 0;
  if (readPointer && readPointer(x, y)) {
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

}  // namespace

void pointerBegin(bool (*read)(int16_t& x, int16_t& y)) {
  readPointer = read;
  // Once only, whatever read was: a second call swaps the reader.
  if (pointer || !read) return;
  pointer = lv_indev_create();
  if (!pointer) return;
  lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(pointer, readCb);
}

bool hasPointer() { return pointer != nullptr && readPointer != nullptr; }

void setTouchOnly(bool on) { onlyTouch = on; }
bool touchOnly() { return onlyTouch; }

void setKeySink(void (*sink)(const rc::KeyEvent&)) { keySink = sink; }

void emit(const rc::KeyEvent& e) {
  if (keySink) keySink(e);
}

}  // namespace lvui
