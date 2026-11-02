// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from wadamesh variants/tdisplay_p4/RM69A10Display.{h,cpp}.
#pragma once
#if defined(BOARD_TDISPLAY_P4)

#include <cstdint>

// The RM69A10 AMOLED, 568x1232 RGB565 over MIPI-DSI. The DPI panel scans one
// frame buffer in PSRAM out continuously; the board writes it and flushes the
// rows it touched out of the cache.
namespace tdp4::panel {

// The XL9535 powered up first. Logs the failing stage and returns false.
bool begin();
uint16_t* frame();  // PANEL_W x PANEL_H, nullptr before begin
// Writes rows [ny0, ny1) of frame() back from the cache, for the DSI to read.
void flush(int ny0, int ny1);
void brightness(uint8_t level);  // DCS 0x51
void sleep(bool sleep);

}  // namespace tdp4::panel

#endif
