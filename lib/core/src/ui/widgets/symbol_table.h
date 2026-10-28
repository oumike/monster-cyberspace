#pragma once

#include <cstdint>
#include <string>

#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// The symbol table a keyed board opens while typing (Action::Symbols), after
// camillia-mt's symbol tray: a grid of punctuation picked with the d-pad that
// inserts one symbol and closes. An optional last row hands over to the
// screen's own palette, which shares the key on the M9. Board-agnostic.
class SymbolTable {
 public:
  static constexpr int kCols = 8;
  enum class Kind : uint8_t {
    None,         // the table used the key (or ignored it) and is still open
    Insert,       // type `ch`; the table closed
    Commands,     // open the screen's palette; the table closed
    Closed,       // Back: the table closed
    PassThrough,  // the table closed; hand the key on as usual
  };
  struct Out {
    Kind kind = Kind::None;
    char32_t ch = 0;
  };

  static const std::u32string& symbols();

  // commands: the label of the palette row, or empty for none.
  void open(std::u32string commands);
  void close() { open_ = false; }
  bool isOpen() const { return open_; }
  int selected() const { return sel_; }

  // Arrows move (wrapping), Select picks, Back closes, a repeated Symbols is
  // ignored (the M9 reports its key more than once per press); any other key
  // closes the table and passes through.
  Out key(const KeyEvent& k);

  // Centred above row `bottom`.
  void render(Grid& g, int bottom, const Theme& t) const;

 private:
  int count() const;  // cells, the commands row included
  std::u32string commands_;
  int sel_ = 0;
  bool open_ = false;
};

}  // namespace rc
