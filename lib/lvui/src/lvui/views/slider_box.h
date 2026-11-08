#pragma once

#include <lvgl.h>

#include <string>
#include <vector>

#include "lvui/views/view_util.h"
#include "ui/widgets/slider.h"

namespace lvui {

// rc::Slider as Slider::render draws it, in cells of the box's font: a box
// outlined in dim, centred over its parent, with the title, then either the
// "- ███▒▒▒ +" bar and the value's label, or (openChoices) a line with a dot
// per stop and the names under it; then [Cancel] and [Set]. A backdrop over
// the whole parent takes every tap, as the grid's catch-all target does.
// Taps emit what the grid's targets do: a bar segment or stop
// {Activate, i, Menu, then Select} (long press: then Palette), "-" Left,
// "+" Right, [Cancel] Back and [Set] Select. Hidden while closed.
class SliderBox : public Tappable {
 public:
  SliderBox() = default;
  SliderBox(const SliderBox&) = delete;
  SliderBox& operator=(const SliderBox&) = delete;

  void create(lv_obj_t* parent);
  void sync(const rc::Slider& m);

  lv_obj_t* backdrop() const { return backdrop_; }
  lv_obj_t* box() const { return box_; }
  lv_obj_t* frame() const { return frame_; }
  lv_obj_t* title() const { return title_; }
  lv_obj_t* filled() const { return filled_; }
  lv_obj_t* empty() const { return empty_; }
  lv_obj_t* value() const { return value_; }
  lv_obj_t* dot(int i) const { return dots_[i]; }
  lv_obj_t* name(int i) const { return names_[i]; }
  // The box's top-left cell within the parent.
  int boxCol() const { return c0_; }
  int boxRow() const { return r0_; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  void build(const rc::Slider& m, int cols, int rows);
  void update(const rc::Slider& m);
  lv_obj_t* cellLabel(int col, int row);
  int dotX(int i) const;

  lv_obj_t *backdrop_ = nullptr, *box_ = nullptr;
  lv_obj_t *title_ = nullptr, *minus_ = nullptr, *plus_ = nullptr, *filled_ = nullptr, *empty_ = nullptr;
  lv_obj_t *value_ = nullptr, *cancel_ = nullptr, *set_ = nullptr, *line_ = nullptr, *frame_ = nullptr;
  std::vector<lv_obj_t*> dots_, names_;
  // What the box was built from.
  bool built_ = false;
  int cols_ = -1, rows_ = -1, steps_ = -1, step_ = -1;
  Cell cell_{0, 0};
  std::u32string titleText_;
  std::vector<std::u32string> nameTexts_;
  // What it shows.
  int shown_ = -1;
  uint16_t bg_ = 0, fg_ = 0, dim_ = 0, sel_ = 0;
  // Geometry in cells, box-relative but c0_/r0_.
  int c0_ = 0, r0_ = 0, w_ = 0, h_ = 0, n_ = 0, bx_ = 0, barW_ = 0, lineW_ = 0;
  bool choices_ = false;
};

}  // namespace lvui
