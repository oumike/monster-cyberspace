#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "input/key_event.h"
#include "spell/spell.h"
#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// Spell suggestions for the word just finished, as camillia-mt's compose box
// does them. Typing a space or punctuation after a word the list doesn't know
// opens a one-row strip: the word as typed, then up to three suggestions. Up
// and Down (the Pager's wheel) swap the word in the text for the next or
// previous option as they go; Select keeps the one shown, and a tap on an
// option (Action::Spell) takes it. Anything else closes the strip and does
// its usual job, so Select on an untouched strip still sends.
//
// F is InputLine or TextArea: text(), cursor() and replace().
class SpellStrip {
 public:
  // Call after each typed character. Closes the strip, then with `on` opens
  // it if that character finished an unknown word.
  template <class F>
  void typed(const F& f, const spell::Dict& d, bool on) {
    hide();
    if (on) open(f.text(), f.cursor(), d);
  }

  // Call before a screen handles a key; true when the strip used it up.
  template <class F>
  bool key(const KeyEvent& k, F& f) {
    if (!visible()) return false;
    switch (k.action) {
      case Action::None:
        return false;
      case Action::Up:
      case Action::Down: {
        const int n = shown_;
        return pick((sel_ + (k.action == Action::Down ? 1 : n - 1)) % n, f);
      }
      case Action::Spell:
        if (k.index >= 0 && k.index < shown_) pick(k.index, f);
        hide();
        return true;
      case Action::Select: {
        const bool used = sel_ != 0;
        hide();
        return used;
      }
      default:
        hide();
        return false;
    }
  }

  bool visible() const { return count_ > 0; }
  void hide() { count_ = shown_ = sel_ = 0; }
  int count() const { return count_; }
  int selected() const { return sel_; }
  const std::u32string& option(int i) const { return opts_[i]; }

  // Draws the strip across `row` and registers a tap per option. Options
  // that don't fit are dropped from the end, keeping at least one suggestion.
  void render(Grid& g, int row, const Theme& t);

 private:
  void open(std::u32string_view text, size_t cursor, const spell::Dict& d);
  bool stillThere(std::u32string_view text) const;

  // Swaps the word for option i. False, with the strip closed, when the word
  // is no longer where it was (the caret moved and something else edited).
  template <class F>
  bool pick(int i, F& f) {
    if (i == sel_) return true;
    if (!stillThere(f.text())) {
      hide();
      return false;
    }
    if (!f.replace(start_, opts_[sel_].size(), opts_[i])) {
      hide();
      return true;
    }
    sel_ = i;
    return true;
  }

  std::u32string opts_[spell::kMaxSuggest + 1];
  int count_ = 0, shown_ = 0, sel_ = 0;
  size_t start_ = 0;  // where the word starts in the text
};

}  // namespace rc
