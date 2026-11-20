// SPDX-License-Identifier: GPL-3.0-or-later
// The CardputerZero's two ways to run (spec 2026-11-18 §3): on its LCD, or
// in a window on the desktop it shows on a monitor. No SDL here, so the
// Linux tests build it alone.
#pragma once

#include <string>

namespace linuxhal {

enum class DisplayMode { Lcd, Window };

// force is MONSTER_DISPLAY ("lcd" or "window" win; anything else is
// ignored); else a Wayland or X display means a window. Null means unset.
DisplayMode chooseDisplayMode(const char* force, const char* wayland, const char* x11);

// Takes a non-blocking flock on path (made if missing). The open descriptor,
// kept for the process's life; kLockHeld if another holds it, or
// kLockUnavailable if the file can't be opened or locked.
constexpr int kLockHeld = -1;
constexpr int kLockUnavailable = -2;
int lockInstance(const std::string& path);

// The lock on ~/.config/monster-cyberspace/.lock: a second Monster prints
// "[boot] already running" and exits 0.
void lockOrExit();

// The desktop menu entry. The store allows no files in usr/share/applications,
// so the package keeps it in /usr/share/monster-cyberspace/ and each start
// copies it to the user's applications directory. Copies src to dst (making
// dst's directory) unless dst already holds the same text; true when dst
// then matches src.
bool installMenuEntry(const std::string& src, const std::string& dst);
// installMenuEntry from the package's copy to
// ${XDG_DATA_HOME:-~/.local/share}/applications/monster-cyberspace.desktop.
void installMenuEntryForUser();

// The mode the board runs in, set by its begin(); the shared flags ask it.
bool windowed();

}  // namespace linuxhal
