#pragma once

#include <lvgl.h>

#include <cstdint>
#include <string>

#include "lvui/views/view_util.h"
#include "ui/widgets/banner.h"

namespace lvui {

// rc::Banner as a one-row label, hidden when the banner isn't visible: hint
// colours, an error filled in the error colour (outlined themes: error text
// in an error box). A tap hides it (no key, as touch_resolve's hideBanner):
// the view calls the onHide hook, which hides the model.
class Banner : public Tappable {
 public:
  Banner() = default;
  Banner(const Banner&) = delete;
  Banner& operator=(const Banner&) = delete;

  void create(lv_obj_t* parent);
  void setOnHide(void (*onHide)()) { onHide_ = onHide; }
  void sync(const rc::Banner& m, int64_t nowMs);

  lv_obj_t* obj() const { return row_; }
  lv_obj_t* label() const { return label_; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  lv_obj_t *row_ = nullptr, *label_ = nullptr;
  void (*onHide_)() = nullptr;
  // What the local colours were set from.
  uint16_t bg_ = 0, fg_ = 0;
  int8_t box_ = -1;
};

}  // namespace lvui
