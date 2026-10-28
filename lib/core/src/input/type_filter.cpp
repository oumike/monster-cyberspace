#include "input/type_filter.h"

namespace rc {

namespace {
char32_t fold(char32_t c) { return c >= U'A' && c <= U'Z' ? c - U'A' + U'a' : c; }
}  // namespace

bool TypeFilter::key(const KeyEvent& k) {
  switch (k.action) {
    case Action::Char:
      if (k.ch < 0x20 || k.ch == 0x7F || (k.ch == U' ' && text_.empty())) return false;
      text_.push_back(k.ch);
      return true;
    case Action::Backspace:
      if (text_.empty()) return false;
      text_.pop_back();
      return true;
    case Action::Back:
      if (text_.empty()) return false;
      text_.clear();
      return true;
    default:
      return false;
  }
}

bool TypeFilter::matches(std::u32string_view name) const {
  if (text_.empty()) return true;
  if (text_.size() > name.size()) return false;
  for (size_t i = 0; i + text_.size() <= name.size(); ++i) {
    size_t j = 0;
    while (j < text_.size() && fold(name[i + j]) == fold(text_[j])) ++j;
    if (j == text_.size()) return true;
  }
  return false;
}

}  // namespace rc
