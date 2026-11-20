// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Whether bmorcelli's Launcher installed this firmware (its own partition
// table: no second OTA slot of ours, no mascot partition). Read once from the
// partition table; rc::launcherHosted decides.
namespace launcher {

bool hosted();

}  // namespace launcher
