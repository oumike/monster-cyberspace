#include "screens/backup_picker.h"

#include <utility>

#include "app/ui_task.h"
#include "screens/draw.h"
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

void BackupPicker::onKey(const KeyEvent& k) {
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
    case Action::Back:
    case Action::Left:
      close();
      break;
    case Action::Select:
    case Action::Right: {
      const int i = list_.selected();
      const std::string name = (i >= 0 && size_t(i) < files_.size()) ? files_[size_t(i)].name : "";
      confirmBox_.confirm(utf8ToU32("Restore " + name + " and restart?"), U"Restore");
      confirming_ = true;
      break;
    }
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
  if (confirming_) {
    confirmBox_.render(g, top, bottom, 0, OverlayPlace::Bottom, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF confirm  back: cancel");
  } else {
    list_.render(g, top + 1, bottom, 1, g.cols() - 2, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF restore  back: cancel");
  }
}

}  // namespace rc
