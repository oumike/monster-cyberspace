#include "screens/backup_picker.h"

#include <utility>

#include "app/ui_task.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/lv_draw.h"
#include "util/utf8.h"

namespace rc {

namespace {
std::string humanSize(size_t bytes) {
  if (bytes < 1024) return std::to_string(bytes) + " B";
  return std::to_string((bytes + 512) / 1024) + " KB";
}
}  // namespace

bool BackupPicker::open() {
  files_.clear();
  const backupstore::Result r = backupstore::listSd(files_);
  if (!r.ok) {
    ui::banner(r.message, true);
    return false;
  }
  if (files_.empty()) {
    ui::banner("No backups on the SD card", false);
    return false;
  }
  std::vector<ListItem> rows;
  rows.reserve(files_.size());
  for (const backupstore::FileInfo& f : files_)
    rows.push_back(ListItem{utf8ToU32(f.name), utf8ToU32(humanSize(f.size))});
  list_.setItems(std::move(rows));
  list_.setSelected(0);
  open_ = true;
  confirming_ = false;
  pad_.reset();
  return true;
}

void BackupPicker::restore() {
  const int i = list_.selected();
  if (i < 0 || size_t(i) >= files_.size()) {
    close();
    return;
  }
  std::string yaml;
  backupstore::Result r = backupstore::readSd(files_[i].name, yaml);
  if (r.ok) r = backupstore::applyAndRestart(yaml);
  for (char& c : yaml) c = '\0';  // the YAML holds secrets
  ui::banner(r.message, !r.ok);
  close();  // on success the device restarts in about 1 s
}

void BackupPicker::ask() {
  const int i = list_.selected();
  const std::string name = (i >= 0 && size_t(i) < files_.size()) ? files_[size_t(i)].name : "";
  confirmBox_.confirm(utf8ToU32("Restore " + name + " and restart?"), U"Restore");
  confirming_ = true;
  pad_.reset();
}

void BackupPicker::onKey(const KeyEvent& k) {
  if (buttons_) {
    const bool leave = k.action == Action::Back ||
                       (k.action == Action::Left && (!board::hasKeys() || pad_.atLeftEdge()));
    if (leave) {
      if (!confirming_) return close();
      confirmBox_.close();
      confirming_ = false;
      pad_.reset();
      return;
    }
    const int i = pad_.key(k);
    if (i < 0) return;
    if (!confirming_) {
      if (size_t(i) >= files_.size()) return;
      list_.setSelected(i);
      return ask();
    }
    confirmBox_.close();
    if (i == 1) return restore();
    confirming_ = false;
    pad_.reset();
    return;
  }
  if (confirming_) {
    switch (confirmBox_.key(k)) {
      case Overlay::Result::None:
        break;
      case Overlay::Result::Closed:
        confirming_ = false;
        break;
      case Overlay::Result::Picked:
        if (confirmBox_.selected() == 1) restore();
        else confirming_ = false;
        break;
    }
    return;
  }
  switch (k.action) {
    case Action::Up:
      list_.up();
      break;
    case Action::Down:
      list_.down();
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && list_.select(k.index)) onKey(KeyEvent{k.then});
      break;
    case Action::Back:
    case Action::Left:
      close();
      break;
    case Action::Select:
    case Action::Right:
      ask();
      break;
    default:
      break;
  }
}

void BackupPicker::render(Grid& g, const Theme& t) {
  const int bottom = g.rows() - 1;
  const int top = 1;
  for (int c = 0; c < g.cols(); ++c) g.put(c, top, U' ', t.barFg, t.barBg);
  g.putText(1, top, U"restore from SD", t.barFg, t.barBg, g.cols() - 2);
  for (int row = top + 1; row < bottom; ++row)
    for (int c = 0; c < g.cols(); ++c) g.put(c, row, U' ', t.fg, t.bg);
  buttons_ = board::hasTouch();
  pad_.setCursor(board::hasKeys());
  if (buttons_) {
    if (confirming_) {
      g.putText(1, top, confirmBox_.title(), t.barFg, t.barBg, g.cols() - 2);
      const std::vector<ListItem> rows{ListItem{U"Cancel", U""},
                                       ListItem{U"Restore", U"", ItemTone::Error}};
      pad_.render(g, t, rows, top + 1, bottom, false);
      drawHint(g, t, U"\u25CF confirm  back: cancel", U"tap: confirm  back: cancel");
    } else {
      pad_.render(g, t, list_.items(), top + 1, bottom, true, 1);
      drawHint(g, t, U"\u2191\u2193 pick  \u25CF restore  back: cancel",
               U"tap to restore  back: cancel");
    }
    return;
  }
  if (confirming_) {
    confirmBox_.render(g, top, bottom, 0, OverlayPlace::Bottom, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF confirm  back: cancel",
             U"tap: confirm  outside: cancel");
  } else {
    list_.render(g, top + 1, bottom, 1, g.cols() - 2, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF restore  back: cancel",
             U"tap to restore  \u2039 cancel");
  }
}

void BackupPicker::build(lv_obj_t* root) {
  layer_ = lvui::plainBox(root);
  lv_obj_set_size(layer_, lv_pct(100), lv_pct(100));
  panel_ = lvui::plainBox(layer_);
  lvui::addStyle(panel_, lvui::Role::Screen);
  titleRow_ = lvui::plainBox(layer_);
  lvui::addStyle(titleRow_, lvui::Role::Bar);
  title_ = lvui::lineLabel(titleRow_);
  listView_.create(layer_);
  padView_.create(layer_);
  hint_.create(layer_);
  confirmView_.create(layer_);
  sync();
}

void BackupPicker::sync() {
  lvui::setHidden(layer_, !open_);
  if (!open_) return;
  lv_obj_update_layout(layer_);
  const lvui::Cell c = lvui::cellOf(layer_);
  if (c.w <= 0 || c.h <= 0) return;
  const int cols = lv_obj_get_width(layer_) / c.w, rows = lv_obj_get_height(layer_) / c.h;
  const int bottom = rows - 1;
  const int top = 1;
  placeCells(panel_, 0, top, cols, bottom);
  placeCells(titleRow_, 0, top, cols, top + 1);
  lvui::place(title_, c.w, 0, (cols - 2) * c.w, c.h);
  buttons_ = board::hasTouch();
  pad_.setCursor(board::hasKeys());
  lvui::setHidden(padView_.obj(), !buttons_);
  lvui::setHidden(listView_.obj(), buttons_ || confirming_);
  if (buttons_) {
    lvui::setHidden(confirmView_.backdrop(), true);
    if (confirming_) {
      lvui::setText(title_, lvui::fitCells(confirmBox_.title(), cols - 2));
      const std::vector<ListItem> rows{ListItem{U"Cancel", U""},
                                       ListItem{U"Restore", U"", ItemTone::Error}};
      pad_.sync(padView_, rows, top + 1, bottom, false);
      syncHint(hint_, U"\u25CF confirm  back: cancel", U"tap: confirm  back: cancel");
    } else {
      lvui::setText(title_, U"restore from SD");
      pad_.sync(padView_, list_.items(), top + 1, bottom, true, 1);
      syncHint(hint_, U"\u2191\u2193 pick  \u25CF restore  back: cancel", U"tap to restore  back: cancel");
    }
    return;
  }
  lvui::setText(title_, U"restore from SD");
  if (confirming_) {
    confirmView_.setPlace(OverlayPlace::Bottom, bottom * c.h, 0, top * c.h);
    confirmView_.sync(confirmBox_);
    syncHint(hint_, U"\u2191\u2193 pick  \u25CF confirm  back: cancel", U"tap: confirm  outside: cancel");
  } else {
    confirmView_.sync(confirmBox_);
    placeCells(listView_.obj(), 1, top + 1, cols - 2, bottom);
    listView_.sync(list_);
    syncHint(hint_, U"\u2191\u2193 pick  \u25CF restore  back: cancel", U"tap to restore  \u2039 cancel");
  }
}

}  // namespace rc
