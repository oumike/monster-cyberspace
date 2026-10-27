#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// One-row text editor. Scrolls horizontally to keep the cursor visible.
class InputLine {
 public:
  static constexpr size_t kMaxBytes = 2048;

  // Refuses a character that would push the UTF-8 length past kMaxBytes.
  void insert(char32_t c);
  void backspace();
  bool left();
  // False when the cursor is already at the end.
  bool right();
  bool atEnd() const { return cursor_ == text_.size(); }
  void clear();
  // Replaces the text (truncated to kMaxBytes) and moves the cursor to the end.
  void setText(std::u32string_view s);
  std::u32string text() const { return text_; }
  std::string utf8() const;
  size_t cursor() const { return cursor_; }
  // Moves the cursor to `pos`, clamped to the text length.
  void setCursor(size_t pos);
  // The screen's field id, carried in the tap Field target.
  void setFieldId(uint8_t id) { fieldId_ = id; }
  // While disabled the text is kept but drawn dim, with no cursor.
  void setDisabled(bool d) { disabled_ = d; }
  bool disabled() const { return disabled_; }
  void render(Grid& g, int row, const Theme& t, std::u32string_view prompt = U"> ") const;

 private:
  std::u32string text_;
  size_t cursor_ = 0, bytes_ = 0;
  bool disabled_ = false;
  uint8_t fieldId_ = 0;
};

}  // namespace rc
