#pragma once

#include <cstdint>

// The T-Display P4's RM69A10 AMOLED over MIPI-DSI, through LovyanGFX
// (camillia-mt's tdisplay_p4_display.h). The panel stays in its native
// portrait; pushRows turns and doubles the renderer's rows in software.
namespace p4panel {

bool begin();
// Rows [y0, y1) of a frameW x frameH frame laid out for rotation r.
void pushRows(const uint16_t* frame, int frameW, int frameH, int y0, int y1, uint8_t r);
// 0 is dark; the AMOLED keeps scanning, so this is also how it sleeps.
void brightness(uint8_t level);
void clear();

}  // namespace p4panel
