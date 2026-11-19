// SPDX-License-Identifier: GPL-3.0-or-later
// What the CardputerZero HAL's files share (spec 2026-11-16 §5). Only
// src/hal/cardputer_zero/ includes this.
#pragma once

#include <cstdint>

namespace cp0 {

constexpr int kW = 320;  // the panel
constexpr int kH = 170;
// The UI is drawn this far in from the sides and the top, which the case's
// bezel covers (none at the bottom); the margin repeats the frame's edge pixels.
constexpr int kInsetX = 2;
constexpr int kInsetTop = 2;
constexpr int kUiW = kW - 2 * kInsetX;  // board::panelWidth()
constexpr int kUiH = kH - kInsetTop;    // board::panelHeight()

uint32_t nowMs();

// fb_cardputer_zero.cpp: the panel's framebuffer. fbOpen picks the node
// (LV_LINUX_FBDEV_DEVICE, else /dev/fb_lcd, else panel-mipi-dbid or
// fb_st7789v from /proc/fb, else /dev/fb0) and maps it; false if none opens.
// fbPush copies rows [y0, y1) of an RGB565 kUiW x kUiH frame inset on the
// panel, converting to the node's depth. While fbDark, pushes only
// update the kept copy and the panel stays black; fbDark(false) shows it.
bool fbOpen();
void fbPush(const uint16_t* frame, int y0, int y1);
void fbDark(bool dark);
void fbClose();  // black, then unmapped

// power_cardputer_zero.cpp: battery and backlight sysfs.
void powerBegin();
bool hasBacklightNode();
// board_cardputer_zero.cpp: the level backlightLevel last set (the
// brightness keys step from it until Settings sets another).
int currentBacklight();
void backlightWrite(uint8_t level);  // 0..255 of the node's max
void powerEnd();                     // the launcher's brightness back

// input_cardputer_zero.cpp: the evdev reader thread. In a desktop window
// (linuxhal::windowed) the built-in keyboard is grabbed while the window has
// focus (inputFocus) and its keys dropped while it hasn't; queueKey adds
// another keyboard's key (SDL's, as an evdev code) to the same queue.
void inputBegin();
void inputEnd();
void inputFocus(bool focused);
void queueKey(uint16_t code, uint8_t value, uint32_t ms);
bool escQuit();

// audio_cardputer_zero.cpp: beeps on ALSA's default PCM, on their own thread.
void audioBegin();
void audioEnd();
void audioStepVolume(int deltaPct);
void audioToggleMute();

}  // namespace cp0
