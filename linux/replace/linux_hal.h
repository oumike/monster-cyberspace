// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the Linux builds' main() needs from the HAL beyond board.h. Both
// Linux HALs (src/hal/cardputer_zero/, src/hal/linux_sim/) implement it.
namespace linuxhal {

// Runs on the main thread after board::begin() until quit() is true or the
// board asks to stop (the simulator's window closed). The simulator presents
// frames and reads SDL events here (macOS lets only the main thread do
// that); the device just waits.
void runMainLoop(bool (*quit)());

// Lets go of the display and input before the process exits (the device
// leaves the framebuffer black for the launcher).
void end();

}  // namespace linuxhal
