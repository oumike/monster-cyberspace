// SPDX-License-Identifier: GPL-3.0-or-later
// What the CardputerZero HAL's files share (spec 2026-11-16 §5). Only
// src/hal/cardputer_zero/ includes this.
#pragma once

#include <cstdint>

namespace cp0 {

constexpr int kW = 320;
constexpr int kH = 170;

uint32_t nowMs();

// fb_cardputer_zero.cpp: the panel's framebuffer. fbOpen picks the node
// (LV_LINUX_FBDEV_DEVICE, else fb_st7789v from /proc/fb, else /dev/fb0) and
// maps it; false if none opens. fbPush copies rows [y0, y1) of an RGB565
// kW x kH frame, converting to the node's depth. While fbDark, pushes only
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

// input_cardputer_zero.cpp: the evdev reader thread.
void inputBegin();
void inputEnd();
bool escQuit();

// audio_cardputer_zero.cpp: beeps on ALSA's default PCM, on their own thread.
void audioBegin();
void audioEnd();
void audioStepVolume(int deltaPct);
void audioToggleMute();

}  // namespace cp0
