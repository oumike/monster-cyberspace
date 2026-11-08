#pragma once

#include <lvgl.h>

#include <string>

#include "lvui/views/view_util.h"
#include "ui/widgets/input_line.h"

namespace lvui {

// rc::InputLine as one row on inputBg, laid out as InputLine::render lays it
// out: [emoji button] prompt text, scrolled sideways so the cursor's cell
// shows, that cell in Cursor colours (none while disabled, when the text is
// dim). Editing stays in the model: this only draws it. A tap on the text
// emits {Focus, text position under the finger, field id}; on the emoji
// button (touch boards, rc::InputLine::setEmojiButton) Action::Emoji.
class InputField : public Tappable {
 public:
  InputField() = default;
  InputField(const InputField&) = delete;
  InputField& operator=(const InputField&) = delete;

  // A full-width row one cell high.
  void create(lv_obj_t* parent);
  void sync(const rc::InputLine& m, std::u32string_view prompt = U"> ");

  lv_obj_t* obj() const { return row_; }
  lv_obj_t* emoji() const { return emoji_; }
  lv_obj_t* prompt() const { return prompt_; }
  // The text before the cursor, the cursor's cell and the text after it.
  lv_obj_t* before() const { return before_; }
  lv_obj_t* cursor() const { return cursor_; }
  lv_obj_t* after() const { return after_; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  void layout(const rc::InputLine& m, std::u32string_view prompt, int cols, Cell cell);

  lv_obj_t *row_ = nullptr, *emoji_ = nullptr, *prompt_ = nullptr;
  lv_obj_t *before_ = nullptr, *cursor_ = nullptr, *after_ = nullptr;
  // What the row was last laid out from.
  std::u32string text_, promptText_;
  size_t cursorPos_ = 0;
  bool disabled_ = false, emojiOn_ = false, laidOut_ = false;
  int cols_ = -1;
  Cell cell_{0, 0};
  uint16_t fg_ = 0;
  // The text field: its first column, width in columns, and the positions
  // under it (as the tap map's Field target).
  int fieldCol_ = 0, avail_ = 0;
  size_t first_ = 0, len_ = 0;
  uint8_t field_ = 0;
};

}  // namespace lvui
