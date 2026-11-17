// SPDX-License-Identifier: GPL-3.0-or-later
// What the CardputerZero has and hasn't. The simulator (src/hal/linux_sim/)
// builds this file too, so both show the same UI.
#if defined(BOARD_CARDPUTER_ZERO) || defined(BOARD_LINUX_SIM)

#include <FS.h>
#include <Preferences.h>

#include <memory>

#include "hal/board.h"
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

bool hasTouch() { return false; }
bool readTouch(int16_t&, int16_t&) { return false; }
bool canRotate() { return false; }
void setRotation(uint8_t) {}
bool hasKeyboard() { return true; }
const rc::FunctionKey* functionKeys(size_t& count) {
  count = rc::kCardputerZeroFunctionKeyCount;
  return rc::kCardputerZeroFunctionKeys;
}
bool hasKeys() { return true; }
bool backspaceIsBack() { return true; }
bool anyInputWakes() { return true; }
bool doubleTapWakes() { return false; }
bool upDownOnly() { return false; }
bool sideNav() { return false; }
void textEntry(bool) {}
bool eink() { return false; }
// 320x170: Large (10x18 cells) leaves too few rows.
bool hasLargeText() { return false; }

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
