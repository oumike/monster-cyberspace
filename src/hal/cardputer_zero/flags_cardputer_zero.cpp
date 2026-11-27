// SPDX-License-Identifier: GPL-3.0-or-later
// What the CardputerZero has and hasn't, on its LCD or in a desktop window
// (spec 2026-11-18). The simulator (src/hal/linux_sim/) builds this file too,
// so both show the same UI.
#if defined(BOARD_CARDPUTER_ZERO) || defined(BOARD_LINUX_SIM)

#include <FS.h>
#include <Preferences.h>

#include <memory>

#include "hal/board.h"
#include "hal/linux_sim/desktop_mode.h"
#include "hal/linux_sim/sdl_window.h"
#include "input/keymap_cardputer_zero.h"

namespace board {

const char* name() { return "CardputerZero"; }
const char* otaSlug() { return "cardputer-zero"; }

bool hasKbLight() { return false; }
uint8_t kbLightDefault() { return 0; }
void kbLight(uint8_t, bool) {}
bool hasNotifyLight() { return false; }
void notifyLight(uint8_t) {}

const char* wakeHint() { return "press any key to wake"; }

bool hasVolume() { return true; }

// In a window the mouse is the pointer, and the panel is the window's size
// over the zoom.
bool hasTouch() { return linuxhal::windowed(); }
bool readTouch(int16_t& x, int16_t& y) { return linuxhal::windowed() && sdlwin::readPointer(x, y); }
bool pointerIsMouse() { return linuxhal::windowed(); }
bool canZoom() { return linuxhal::windowed(); }
void setZoom(uint8_t zoom) {
  if (linuxhal::windowed()) sdlwin::setZoom(zoom);
}
bool panelSizeChanged() { return linuxhal::windowed() && sdlwin::takePanelSize(); }
// A window has room for Home's status lines, Fluffy and Unread rows, as the
// Tab5; the LCD's 320x170 does not.
bool homeStatus() { return linuxhal::windowed(); }
bool canRotate() { return false; }
void setRotation(uint8_t) {}
bool hasKeyboard() { return true; }
const rc::FunctionKey* functionKeys(size_t& count) {
  count = rc::kCardputerZeroFunctionKeyCount;
  return rc::kCardputerZeroFunctionKeys;
}
bool hasKeys() { return true; }
// Ctrl tapped alone, as the M9's: the actions, then the emoji tray.
bool emojiKeyOpensActions() { return true; }
bool backspaceIsBack() { return true; }
bool anyInputWakes() { return true; }
bool doubleTapWakes() { return false; }
bool upDownOnly() { return false; }
bool sideNav() { return false; }
void textEntry(bool) {}
bool eink() { return false; }
// The LCD's 320x170: Large (10x18 cells) leaves too few rows. A window is
// at least that size times the zoom.
bool hasLargeText() { return linuxhal::windowed(); }

// Linux owns the clock; there is no GPS.
bool hasGps() { return false; }
void gpsPower(bool) {}
size_t gpsRead(uint8_t*, size_t) { return 0; }
bool rtcRead(int64_t&) { return false; }
bool rtcWrite(int64_t) { return false; }

// No SD slot: backups go where the backup store's SD paths point, a folder
// in the home directory (spec section 8), made on first use.
bool hasSdSlot() { return true; }
fs::FS* sdMount() {
  static std::unique_ptr<fs::FS> dir;
  const std::string root = compat::dataDir();
  if (!compat::makeDirs(root)) return nullptr;
  if (!dir || dir->root() != root) dir = std::make_unique<fs::FS>(root, "/monster-cyberspace");
  return dir.get();
}
void sdUnmount() {}
const char* backupPlaceName() { return "~/monster-cyberspace"; }

}  // namespace board

#endif
