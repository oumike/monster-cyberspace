#pragma once

#include <string>
#include <vector>

#include "input/key_event.h"
#include "store/backup_store.h"
#include "ui/grid.h"
#include "ui/theme.h"
#include "ui/widgets/list_view.h"

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
  void render(Grid&, const Theme&);  // fills the rows above the hint, draws its own hint

 private:
  void restore();

  std::vector<backupstore::FileInfo> files_;
  ListView list_, confirmMenu_;
  bool open_ = false;
  bool confirming_ = false;
};

}  // namespace rc
