#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "input/key_event.h"
#include "ui/grid.h"

namespace rc {

// What a tap target does when hit.
enum class TargetKind : uint8_t { None, Row, Fixed, Field, Osk, Banner };

struct Target {
  TargetKind kind = TargetKind::None;
  ListId list = ListId::Main;
  int16_t index = -1;          // Row: the item; Osk: the key; Field (from hit): text position
  KeyEvent tap, hold;          // Fixed; hold.action None means a hold acts as a tap
  uint8_t field = 0;           // Field
  int16_t first = 0, len = 0;  // Field row: positions first..first+len lie under it
};

struct ListInfo {
  ListId list;
  int selected, count;
  bool wraps;
  // A scrolling list (messages, feed entries): on touch-only boards a tap
  // there does nothing and a Hold acts on the item.
  bool holdToAct;
};

// Per-frame hit targets. Widgets fill it as they draw; the touch resolver reads
// the previous frame's map.
class TapMap {
 public:
  void clear();                                 // targets and list infos
  // For the addCells calls that follow; offsetPx moves the grid down (a docked
  // grid under one with a header, Grid::setHeader).
  void setOrigin(CellSize cell, int firstRow, int offsetPx = 0);
  // Inclusive, stored in px. cellW > 0 overrides the cell width, for the wider
  // cells of RowKind::Big rows.
  void addCells(int c0, int r0, int c1, int r1, const Target& t, int cellW = 0);
  // Re-places the rows of targets added with setOrigin(_, firstRow) by the
  // renderer's row spans (rowSpans), so taps land on RowKind::Big rows as
  // drawn, and shifts them right by their first row's shift. Call once the grid is drawn; other grids' targets are left alone.
  void mapRows(int firstRow, const std::vector<RowSpan>& spans);
  void setList(ListId id, int selected, int count, bool wraps, bool holdToAct = false);
  const ListInfo* list(ListId id) const;  // null if not registered this frame
  int visibleRows(ListId id) const;       // Row targets registered for id
  CellSize cell() const { return cell_; }
  size_t size() const { return rects_.size(); }
  // The last-registered target containing (x, y). For a Field, index =
  // first + the cell column under x (from the rect's left), capped at first + len.
  Target hit(int x, int y) const;

 private:
  struct Rect {
    int x0, y0, x1, y1;
    int firstRow, r0, r1;  // the cells the rect was added from
    int top;               // px of the grid's row 0
    Target t;
  };
  std::vector<Rect> rects_;
  std::vector<ListInfo> lists_;
  CellSize cell_{0, 0};
  int firstRow_ = 0, offsetPx_ = 0;
};

TapMap* activeTaps();          // null: no touch, so widgets register nothing
void setActiveTaps(TapMap* m);

}  // namespace rc
