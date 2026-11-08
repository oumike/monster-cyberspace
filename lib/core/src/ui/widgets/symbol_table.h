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
// screen's own palette. Emoji (Action::Emoji): the tray emoji, drawn large
// (kTrayBigCols x kTrayBigRows cells, emoji_set.h) where they all fit, or where
// kMinBigRows rows of them fit and the rest scroll, else two cells wide (and
// scrolling when even those don't fit); the view follows the selection. A pick
// inserts and stays open, Back closes it, and it has the same palette row.
// Board-agnostic.
class SymbolTable {
 public:
  static constexpr int kCols = 8;       // symbols a row
  static constexpr int kEmojiCols = 6;  // emoji a row, at least; more when short
  static constexpr int kMinBigRows = 3;  // large emoji rows shown, at least, when scrolling
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
  // Boards where Backspace doubles as Back: Backspace closes the table.
  void setBackspaceCloses(bool on) { backspaceCloses_ = on; }
  // Builds without the emoji tray: Symbols never switches to it.
  void setEmoji(bool on) { emoji_ = on; }
  void close() { open_ = false; }
  bool isOpen() const { return open_; }
  int selected() const { return sel_; }
  Set set() const { return set_; }
  int cols() const { return set_ == Set::Emoji ? emojiCols_ : kCols; }

  // Arrows move (wrapping), Select picks, Back closes. Symbols in the symbol
  // table switches to the emoji tray (the Pager's Sym pressed again); a repeat
  // of the opening key is ignored (the M9 reports its keys more than once per
  // press). Any
  // other key closes the symbol table and passes through; the emoji tray
  // swallows it.
  Out key(const KeyEvent& k);

  // Where render puts the table in a `gridCols` wide grid, centred above row
  // `bottom`, in cells. Settles the emoji columns, size and scroll as render
  // does, so the arrows move through what is drawn.
  struct Layout {
    int left = 0, top = 0, width = 0, h = 0;  // the box, title row and outline included
    int cols = 0, rows = 0;                   // items a row, item rows in all
    int pitch = 0, rowStep = 0;               // cells from one item to the next across, down
    int ew = 1, eh = 1;                       // an item's cells (a symbol: pitch x 1)
    int shown = 0, scroll = 0;                // item rows shown, the first one
    bool big = false, gaps = false, scrolls = false;
    // Item i's top-left cell (shown when its row is).
    int col(int i) const { return left + 1 + (i % cols) * pitch; }
    int row(int i) const { return top + 1 + (gaps ? 1 : 0) + (i / cols - scroll) * rowStep; }
  };
  Layout layout(int gridCols, int bottom) const;
  // The open set's symbols or emoji, one code point each.
  const std::u32string& items() const { return set_ == Set::Emoji ? emoji() : symbols(); }
  // The commands row's label (empty: none).
  const std::u32string& commands() const { return commands_; }

  // Centred above row `bottom`.
  void render(Grid& g, int bottom, const Theme& t) const;

 private:
  int count() const;  // cells, the commands row included
  std::u32string commands_;
  Set set_ = Set::Symbols;
  bool linear_ = false;
  bool backspaceCloses_ = false;
  bool emoji_ = true;
  int sel_ = 0;
  mutable int emojiCols_ = kEmojiCols;  // set by render to fit the screen
  mutable bool big_ = false;            // set by render: large emoji fit
  mutable int scroll_ = 0;              // set by render: the first emoji row shown
  bool open_ = false;
};

}  // namespace rc
