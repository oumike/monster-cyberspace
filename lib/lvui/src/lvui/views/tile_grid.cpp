#include "lvui/views/tile_grid.h"

#include <algorithm>

#include "lvui/cell_font.h"
#include "lvui/styles.h"
#include "ui/theme.h"

namespace lvui {

namespace {

void setBorder(lv_obj_t* obj, uint16_t rgb565) {
  if (lv_obj_get_style_border_width(obj, LV_PART_MAIN) != 1) lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
  const lv_color_t c = color(rgb565);
  if (!lv_color_eq(lv_obj_get_style_border_color(obj, LV_PART_MAIN), c))
    lv_obj_set_style_border_color(obj, c, LV_PART_MAIN);
}

void setClear(lv_obj_t* obj) {
  if (lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) != LV_OPA_TRANSP)
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
}

bool sameRects(const std::vector<rc::TileRect>& a, const std::vector<rc::TileRect>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].c0 != b[i].c0 || a[i].r0 != b[i].r0 || a[i].c1 != b[i].c1 || a[i].r1 != b[i].r1) return false;
  return true;
}

bool sameFit(const HomeFit& a, const HomeFit& b) {
  return a.big == b.big && a.column == b.column && a.ts == b.ts && a.cell.w == b.cell.w &&
         a.cell.h == b.cell.h && a.lay.shiftPx == b.lay.shiftPx && a.lay.lastRow == b.lay.lastRow &&
         a.lay.pads == b.lay.pads && sameRects(a.lay.tiles, b.lay.tiles);
}

bool sameItems(const std::vector<rc::ListItem>& a, const std::vector<rc::ListItem>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].tone != b[i].tone || a[i].marker != b[i].marker || a[i].label != b[i].label ||
        a[i].right != b[i].right || a[i].detail != b[i].detail)
      return false;
  return true;
}

}  // namespace

HomeFit homeFit(int panelW, int areaPx, int gridRows, rc::TextSize ts, int count, int percent,
                bool spanFirst) {
  HomeFit f;
  f.ts = ts;
  // Gapped tiles first, big then normal rows; touching tiles only when no
  // gapped layout fits.
  for (bool gaps : {true, false}) {
    for (bool big : {true, false}) {
      const rc::CellSize cell = big ? rc::bigCellSize(ts) : rc::cellSize(ts);
      f.lay = rc::homeLayout(panelW, areaPx, gridRows, cell, count, percent, gaps, spanFirst);
      if (!f.lay.tiles.empty()) {
        f.big = big;
        f.cell = cell;
        return f;
      }
    }
  }
  // One scrolling column, gapped while that still shows three tiles.
  f.column = true;
  const int want = std::min(3, count);
  rc::HomeLayout best;
  bool bestBig = false;
  for (bool gaps : {true, false}) {
    for (bool big : {true, false}) {
      rc::HomeLayout c = rc::homeColumn(panelW, areaPx, gridRows, big ? rc::bigCellSize(ts) : rc::cellSize(ts),
                                        count, percent, gaps);
      if (c.tiles.size() > best.tiles.size()) {
        best = c;
        bestBig = big;
      }
      if (static_cast<int>(c.tiles.size()) >= want) break;
    }
    if (static_cast<int>(best.tiles.size()) >= want) break;
  }
  f.lay = best;
  f.big = bestBig;
  f.cell = bestBig ? rc::bigCellSize(ts) : rc::cellSize(ts);
  return f;
}

void TileGrid::create(lv_obj_t* parent, rc::ListId list) {
  root_ = plainBox(parent);
  lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
  sbHost_ = plainBox(root_);
  bar_.create(sbHost_);
  gestures_.attach(root_, this, list, false);
  mode_ = Mode::None;
}

void TileGrid::syncTiles(const std::vector<Tile>& tiles, const lv_font_t* font, int shownRows, lv_area_t bg) {
  const rc::Theme& t = currentTheme();
  const lv_font_t* f = font ? font : lv_obj_get_style_text_font(root_, LV_PART_MAIN);
  const int lineH = lv_font_get_line_height(f);
  while (tiles_.size() < tiles.size()) {
    TileObj o;
    o.box = plainBox(root_);
    o.inner = plainBox(o.box);
    tiles_.push_back(o);
  }
  for (size_t i = 0; i < tiles_.size(); ++i) {
    TileObj& o = tiles_[i];
    if (i >= tiles.size()) {
      setHidden(o.box, true);
      continue;
    }
    const Tile& s = tiles[i];
    setHidden(o.box, false);
    place(o.box, s.x, s.y, s.w, s.h);
    setBgColor(o.box, t.bg);
    setBorder(o.box, s.edge);
    if (lv_obj_get_style_text_font(o.box, LV_PART_MAIN) != f) lv_obj_set_style_text_font(o.box, f, LV_PART_MAIN);
    bool added = false;
    while (o.labels.size() < s.texts.size()) {
      o.labels.push_back(lineLabel(o.box));
      added = true;
    }
    // The cursor's box goes over the text, as the grid outlines it last.
    if (added) lv_obj_move_foreground(o.inner);
    for (size_t j = 0; j < o.labels.size(); ++j) {
      lv_obj_t* l = o.labels[j];
      setHidden(l, j >= s.texts.size());
      if (j >= s.texts.size()) continue;
      const Text& x = s.texts[j];
      place(l, x.x, x.y, x.w, lineH);
      setText(l, x.text);
      setTextColor(l, x.fg);
      if (x.opaque)
        setBgColor(l, x.bg);
      else
        setClear(l);
    }
    setHidden(o.inner, s.iw <= 0);
    if (s.iw > 0) {
      place(o.inner, s.ix, s.iy, s.iw, s.ih);
      setBorder(o.inner, t.selectBg);
    }
    o.area = {s.x, s.y, s.x + s.w - 1, s.y + s.h - 1};
    o.index = s.index;
  }
  shown_ = static_cast<int>(tiles.size());
  shownRows_ = shownRows;
  bg_ = bg;
}

void TileGrid::syncHome(const HomeFit& fit, int row1Px, const std::vector<HomeTileInfo>& infos, int first,
                        int selected, bool arrows) {
  const bool cursor = rc::cursorShown();
  if (mode_ == Mode::Home && theme_ == themeSerial() && cursor == cursor_ && row1Px == row1_ &&
      first == first_ && selected == sel_ && arrows == arrows_ && sameFit(fit, fit_) && infos == infos_)
    return;
  mode_ = Mode::Home;
  theme_ = themeSerial();
  cursor_ = cursor;
  fit_ = fit;
  row1_ = row1Px;
  first_ = first;
  sel_ = selected;
  arrows_ = arrows;
  infos_ = infos;

  const rc::Theme& t = currentTheme();
  const rc::HomeLayout& lay = fit.lay;
  const int cw = fit.cell.w;
  // Grid row r's top: row 1 at row1Px, pad rows their own height, the rest a cell.
  auto rowY = [&](int r) {
    int y = row1Px;
    for (int k = 1; k < r; ++k) {
      int h = fit.cell.h;
      for (const auto& p : lay.pads)
        if (p.first == k) h = p.second;
      y += h;
    }
    return y;
  };
  const int count = static_cast<int>(infos.size());
  std::vector<Tile> tiles;
  int rows = 0;
  for (int slot = 0; slot < static_cast<int>(lay.tiles.size()) && first + slot < count; ++slot) {
    const int i = first + slot;
    const rc::TileRect& r = lay.tiles[slot];
    const HomeTileInfo& it = infos[i];
    const bool sel = i == selected;
    const int y0 = rowY(r.r0);
    Tile tile{lay.shiftPx + r.c0 * cw, y0, (r.c1 - r.c0 + 1) * cw, rowY(r.r1 + 1) - y0, i,
              sel && cursor ? t.selectBg : t.dim};
    const int mid = (r.r0 + r.r1) / 2;
    const int inW = r.c1 - r.c0 - 1;
    const int ls = static_cast<int>(it.label.size()), ss = static_cast<int>(it.sub.size());
    // A wide column tile has room for the sub line beside the label.
    const bool beside = fit.column && ss > 0 && mid + 1 > r.r1 - 1 && ls + 2 + ss <= inW;
    const int lineW = beside ? ls + 2 + ss : ls;
    const int lw = std::min(ls, inW);
    const int lc = r.c0 + 1 + std::max(0, (inW - std::min(lineW, inW)) / 2);
    auto at = [&](int col, int row, int w, const std::u32string& s, uint16_t fg) {
      tile.texts.push_back(Text{(col - r.c0) * cw, rowY(row) - y0, w * cw, s, fg});
    };
    at(lc, mid, lw, it.label, it.unread ? t.selectBg : t.fg);
    if (beside) at(lc + lw + 2, mid, ss, it.sub, t.dim);
    if (!beside && ss > 0 && mid + 1 <= r.r1 - 1) {
      const int sw = std::min(ss, inW);
      at(r.c0 + 1 + std::max(0, (inW - sw) / 2), mid + 1, sw, it.sub, t.dim);
    }
    // Outlined themes have one ink for both outlines: the cursor is a second box.
    if (sel && cursor && t.outlined && r.r1 - r.r0 >= 2 && r.c1 - r.c0 >= 2) {
      tile.ix = cw;
      tile.iy = rowY(r.r0 + 1) - y0;
      tile.iw = (r.c1 - r.c0 - 1) * cw;
      tile.ih = rowY(r.r1) - rowY(r.r0 + 1);
    }
    rows += fit.column ? r.r1 - r.r0 + 1 : 1;
    tiles.push_back(std::move(tile));
  }
  // More tiles above or below the column: arrows on the outline.
  if (fit.column && arrows && !tiles.empty()) {
    const rc::TileRect& top = lay.tiles.front();
    const rc::TileRect& bot = lay.tiles[tiles.size() - 1];
    if (first > 0)
      tiles.front().texts.push_back(Text{(top.c1 - 2 - top.c0) * cw, 0, cw, U"\u25B2", t.fg, true, t.bg});
    if (first + static_cast<int>(tiles.size()) < count)
      tiles.back().texts.push_back(Text{(bot.c1 - 2 - bot.c0) * cw, tiles.back().h - fit.cell.h, cw, U"\u25BC",
                                        t.fg, true, t.bg});
  }
  bar_.sync(0, cellOf(root_), 0, 0, 0, 0);
  syncTiles(tiles, font(fit.ts, fit.big), rows);
}

void TileGrid::syncButtons(const rc::ButtonGrid& b, const std::vector<rc::ListItem>& items, int selected,
                           bool keepRight, int top, int bottom, int topPx, Scroll scroll) {
  const bool cursor = rc::cursorShown();
  if (mode_ == Mode::Buttons && theme_ == themeSerial() && cursor == cursor_ && selected == sel_ &&
      keepRight == keepRight_ && top == top_ && bottom == bottom_ && topPx == topPx_ &&
      scroll.total == scroll_.total && scroll.visible == scroll_.visible && scroll.first == scroll_.first &&
      b.first == grid_.first && sameRects(b.rects, grid_.rects) && sameItems(items, items_))
    return;
  mode_ = Mode::Buttons;
  theme_ = themeSerial();
  cursor_ = cursor;
  sel_ = selected;
  keepRight_ = keepRight;
  top_ = top;
  bottom_ = bottom;
  topPx_ = topPx;
  scroll_ = scroll;
  grid_ = b;
  items_ = items;

  const rc::Theme& t = currentTheme();
  const Cell c = cellOf(root_);
  if (c.w <= 0 || c.h <= 0) return;
  auto rowY = [&](int r) { return topPx + (r - top) * c.h; };
  std::vector<Tile> tiles;
  for (size_t k = 0; k < b.rects.size(); ++k) {
    const int i = b.first + static_cast<int>(k);
    if (i >= static_cast<int>(items.size())) break;
    const rc::TileRect& r = b.rects[k];
    const rc::ListItem& it = items[i];
    const bool sel = i == selected && cursor;
    const int y0 = rowY(r.r0);
    Tile tile{r.c0 * c.w, y0, (r.c1 - r.c0 + 1) * c.w, (r.r1 - r.r0 + 1) * c.h, i, sel ? t.selectBg : t.dim};
    const int mid = r.r0 + 1;  // the label row; a detail goes under it
    const int inW = r.c1 - r.c0 - 1;
    const int lw = static_cast<int>(it.label.size());
    int rw = static_cast<int>(it.right.size());
    if (rw > 0 && lw + 1 + rw > inW)
      rw = keepRight ? std::max(0, inW - 1 - std::min(lw, std::max(inW - 1 - rw, (inW - 1) / 2))) : 0;
    auto at = [&](int col, int row, int w, const std::u32string& s, uint16_t fg) {
      tile.texts.push_back(Text{(col - r.c0) * c.w, rowY(row) - y0, std::max(0, w) * c.w, s, fg});
    };
    const uint16_t fg = it.tone == rc::ItemTone::Error ? t.error : it.marker ? t.selectBg : t.fg;
    at(r.c0 + 1, mid, rw > 0 ? inW - rw - 1 : inW, it.label, fg);
    if (rw > 0) at(r.c1 - rw, mid, rw, it.right, t.dim);
    if (!it.detail.empty()) at(r.c0 + 1, mid + 1, inW, rc::oneLine(it.detail, inW), t.dim);
    // Outlined themes: the cursor is a second box, where it clears the label row.
    if (sel && t.outlined && r.r1 - r.r0 >= 3 && r.c1 - r.c0 >= 2) {
      tile.ix = c.w;
      tile.iy = c.h;
      tile.iw = (r.c1 - r.c0 - 1) * c.w;
      tile.ih = (r.r1 - r.r0 - 1) * c.h;
    }
    tiles.push_back(std::move(tile));
  }
  const int w = lv_obj_get_width(lv_obj_get_parent(root_));
  const int cols = w / c.w;
  place(sbHost_, 0, topPx, cols * c.w, (bottom - top) * c.h);
  bar_.sync(cols * c.w, c, bottom - top, scroll.total, scroll.visible, scroll.first);
  // Under the buttons, the whole area takes drags; a tap there picks nothing.
  syncTiles(tiles, nullptr, 1 + static_cast<int>(tiles.size()),
            lv_area_t{0, topPx, cols * c.w - 1, topPx + (bottom - top) * c.h - 1});
}

int TileGrid::itemAt(lv_point_t p) const {
  lv_area_t a;
  lv_obj_get_coords(root_, &a);
  const int x = p.x - a.x1, y = p.y - a.y1;
  for (int i = shown_ - 1; i >= 0; --i) {
    const lv_area_t& r = tiles_[i].area;
    if (x >= r.x1 && x <= r.x2 && y >= r.y1 && y <= r.y2) return tiles_[i].index;
  }
  return -1;
}

bool TileGrid::onRows(lv_point_t p) const {
  if (itemAt(p) >= 0) return true;
  lv_area_t a;
  lv_obj_get_coords(root_, &a);
  const int x = p.x - a.x1, y = p.y - a.y1;
  return x >= bg_.x1 && x <= bg_.x2 && y >= bg_.y1 && y <= bg_.y2;
}

void TileGrid::missed(bool) {
  if (!missBack_) return;
  rc::KeyEvent e;
  e.action = rc::Action::Back;
  emit(e);
}


}  // namespace lvui
