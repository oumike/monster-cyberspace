#pragma once

#include <cstdint>
#include <string>

#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// The pickers a keyed board opens while typing, after camillia-mt's trays.
// Symbols (Action::Symbols): a grid of punctuation picked with the d-pad that
// inserts one symbol and closes; an optional last row hands over to the
// screen's own palette. Emoji (Action::Emoji): the tray emoji, two cells wide;
// a pick inserts and stays open, Back closes it, and it has the same palette
// row. Board-agnostic.
class SymbolTable {
 public:
  static constexpr int kCols = 8;       // symbols a row
  static constexpr int kEmojiCols = 6;  // emoji a row, spaced out
  enum class Kind : uint8_t {
    None,         // the table used the key (or ignored it) and is still open
    Insert,       // type `ch`; the table closed
    Commands,     // open the screen's palette; the table closed
    Closed,       // Back: the table closed
    PassThrough,  // the table closed; hand the key on as usual
  };
  enum class Set : uint8_t { Symbols, Emoji };
  struct Out {
    Kind kind = Kind::None;
    char32_t ch = 0;
  };

  static const std::u32string& symbols();
  static const std::u32string& emoji();

  // commands: the label of the palette row, or empty for none.
  // linear: Up/Down step one cell, wrapping, for boards with only a wheel.
  void open(std::u32string commands, Set set = Set::Symbols, bool linear = false);
  void close() { open_ = false; }
  bool isOpen() const { return open_; }
  int selected() const { return sel_; }
  Set set() const { return set_; }
  int cols() const { return set_ == Set::Emoji ? kEmojiCols : kCols; }

  // Arrows move (wrapping), Select picks, Back closes. Symbols in the symbol
  // table switches to the emoji tray (the Pager's Sym pressed again); a repeat
  // of the opening key is ignored (the M9 reports its keys more than once per
  // press). Any
  // other key closes the symbol table and passes through; the emoji tray
  // swallows it.
  Out key(const KeyEvent& k);

  // Centred above row `bottom`.
  void render(Grid& g, int bottom, const Theme& t) const;

 private:
  int count() const;  // cells, the commands row included
  const std::u32string& items() const { return set_ == Set::Emoji ? emoji() : symbols(); }
  std::u32string commands_;
  Set set_ = Set::Symbols;
  bool linear_ = false;
  int sel_ = 0;
  bool open_ = false;
};

}  // namespace rc
