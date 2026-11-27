// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

// A panel drawn into a desktop window (the CardputerZero's window mode, spec
// 2026-11-18): the window's drawable pixels hold the panel scaled up by a
// whole zoom, centred, with a black margin taking what is left over.
namespace rc::winfit {

constexpr uint8_t kZoomAuto = 0;  // the setting's Auto: the zoom follows the window
constexpr uint8_t kZoomMin = 1;
constexpr uint8_t kZoomMax = 3;
constexpr uint8_t kZoomDefault = kZoomAuto;
// Auto picks the biggest whole zoom (up to kAutoZoomMax) that still leaves a
// panel of at least kAutoPanelW x kAutoPanelH, else 1.
constexpr int kAutoPanelW = 800;
constexpr int kAutoPanelH = 450;
constexpr int kAutoZoomMax = 6;
constexpr int kWheelLines = 3;  // Up/Down key presses per wheel notch

constexpr uint8_t clampZoom(int z) { return uint8_t(z < kZoomMin ? kZoomMin : z > kZoomMax ? kZoomMax : z); }
// A saved or entered zoom setting: Auto, or 1-3 clamped.
constexpr uint8_t zoomSetting(int z) { return z == kZoomAuto ? kZoomAuto : clampZoom(z); }

// The zoom Auto picks for a drawW x drawH drawable.
int autoZoom(int drawW, int drawH);

struct Fit {
  int panelW = 1, panelH = 1;  // the panel, in its own pixels
  int offX = 0, offY = 0;      // its top left corner in the drawable
  int zoom = 1;
};

// The panel that fits a drawW x drawH drawable at zoom (Auto, or clamped),
// at least 1x1.
Fit fit(int drawW, int drawH, int zoom);

// A drawable point as a panel point, clamped to the panel; false if it was
// outside it (in the margin or beyond the window).
bool toPanel(const Fit& f, int drawX, int drawY, int16_t& x, int16_t& y);

// The smallest window: the LCD's panel at zoom (clamped; Auto counts as 1).
void minWindow(int lcdW, int lcdH, int zoom, int& w, int& h);

// Up (negative) or Down key presses for wheel notches (positive: towards the user).
constexpr int wheelSteps(int notches) { return notches * kWheelLines; }

}  // namespace rc::winfit
