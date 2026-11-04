#pragma once
#if defined(BOARD_TANMATSU)

#include <cstdint>

// The ST7701 over MIPI-DSI from one PSRAM frame buffer (PANEL_W x PANEL_H,
// RGB565), written by the caller and flushed out of the cache by rows.
namespace tanmatsu::panel {

bool begin();
uint16_t* frame();
// Native rows [y0, y1) written since the last flush.
void flush(int y0, int y1);
void sleep(bool sleep);

}  // namespace tanmatsu::panel

#endif
