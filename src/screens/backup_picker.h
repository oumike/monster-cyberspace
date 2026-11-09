#pragma once

#include <string>
#include <vector>

#include <lvgl.h>

#include "input/key_event.h"
#include "lvui/tree_views.h"
#include "lvui/views/cell_text.h"
#include "lvui/views/list_box.h"
#include "lvui/views/menu_overlay.h"
#include "lvui/views/tile_grid.h"
#include "screens/button_pad.h"
#include "store/backup_store.h"
#include "ui/grid.h"
#include "ui/theme.h"
#include "ui/widgets/list_view.h"
#include "ui/widgets/overlay.h"

namespace rc {

// A list overlay for the SD-card backups, then a "Restore <name> and restart?"
// confirm. Shared by the Settings and Setup screens. Every SD access runs on
// the UI task from a key press.
class BackupPicker {
 public:
  // Lists the card. Shows a banner and returns false on "No SD card" or
  // "No backups on the SD card".
  bool open();
  bool isOpen() const { return open_; }
  void close() { open_ = confirming_ = false; }
  // Up/Down pick; Select/Right on a file -> confirm. Back/Left leaves the
  // confirm, then the list.
  void onKey(const KeyEvent&);
  // LVGL: its objects over root's rows below the status bar, hidden while
  // closed. Build it after the screen's own views, so it covers them.
  void build(lv_obj_t* root);
  void sync();

 private:
  void restore();
  void ask();

  std::vector<backupstore::FileInfo> files_;
  ListView list_;
  Overlay confirmBox_;
  bool open_ = false;
  bool confirming_ = false;
  // Touch boards: the files, then Cancel / Restore, as buttons.
  ButtonPad pad_;
  bool buttons_ = false;
  // LVGL: a transparent layer as big as the screen holding the panel, its
  // title row, the list or buttons, the confirm and the hint.
  lv_obj_t *layer_ = nullptr, *panel_ = nullptr, *titleRow_ = nullptr, *title_ = nullptr;
  // The views, on the heap only while their tree is up (lvui/tree_views.h).
  struct Views {
    lvui::ListBox listView_;
    lvui::TileGrid padView_;
    lvui::MenuOverlay confirmView_;
    lvui::HintBar hint_;
  };
  lvui::TreeViews<Views> v_;
};

}  // namespace rc
